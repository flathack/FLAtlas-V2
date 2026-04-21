// rendering/preview/ModelViewerPage.cpp - standalone 3D model viewer page built on the reusable 3D viewport

#include "ModelViewerPage.h"

#include "ModelPreview.h"
#include "rendering/view3d/ModelViewport3D.h"
#include "core/EditingContext.h"
#include "core/PathUtils.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QTimer>

#include <utility>

namespace flatlas::rendering {

namespace {

QString currentDataPath()
{
    return flatlas::core::PathUtils::ciResolvePath(
        flatlas::core::EditingContext::instance().primaryGamePath(),
        QStringLiteral("DATA"));
}

QString normalizedModelLookupKey(const QString &path)
{
    return QDir::fromNativeSeparators(path).trimmed().toLower();
}

} // namespace

ModelViewerPage::ModelViewerPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_titleLabel = new QLabel(tr("3D Model Viewer"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 700;"));
    layout->addWidget(m_titleLabel);

    m_loadTimer = new QTimer(this);
    m_loadTimer->setSingleShot(true);
    connect(m_loadTimer, &QTimer::timeout, this, &ModelViewerPage::executeScheduledViewportLoad);

    auto *topRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search model, archetype, category or path"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { rebuildTree(); });
    topRow->addWidget(m_searchEdit, 1);

    m_summaryLabel = new QLabel(this);
    topRow->addWidget(m_summaryLabel);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, [this]() { rebuildEntries(); });
    topRow->addWidget(m_refreshButton);

    m_loadFileButton = new QPushButton(tr("Open Model File..."), this);
    connect(m_loadFileButton, &QPushButton::clicked, this, &ModelViewerPage::loadArbitraryModel);
    topRow->addWidget(m_loadFileButton);
    layout->addLayout(topRow);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter, 1);

    auto *leftPane = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_tree = new QTreeWidget(leftPane);
    m_tree->setHeaderLabels({tr("Model"), tr("Archetype"), tr("Render")});
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        auto *item = m_tree->currentItem();
        if (!item) {
            setCurrentEntry(nullptr);
            return;
        }
        const QString modelPath = item->data(0, Qt::UserRole).toString();
        setCurrentEntry(findEntryByModelPath(modelPath));
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        if (!item)
            return;
        const QString modelPath = item->data(0, Qt::UserRole).toString();
        if (const auto *entry = findEntryByModelPath(modelPath))
            loadEntryIntoViewport(entry);
    });
    leftLayout->addWidget(m_tree, 1);
    splitter->addWidget(leftPane);

    auto *rightPane = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto *previewControls = new QHBoxLayout();
    m_resetCameraButton = new QPushButton(tr("Reset Camera"), rightPane);
    connect(m_resetCameraButton, &QPushButton::clicked, this, [this]() {
        if (m_viewport)
            m_viewport->resetView();
    });
    previewControls->addWidget(m_resetCameraButton);

    m_boundsCheck = new QCheckBox(tr("Bounding Box"), rightPane);
    previewControls->addWidget(m_boundsCheck);
    m_wireframeCheck = new QCheckBox(tr("Wireframe"), rightPane);
    previewControls->addWidget(m_wireframeCheck);
    m_meshCheck = new QCheckBox(tr("Mesh"), rightPane);
    m_meshCheck->setChecked(true);
    previewControls->addWidget(m_meshCheck);
    m_whiteBgCheck = new QCheckBox(tr("White BG"), rightPane);
    previewControls->addWidget(m_whiteBgCheck);
    previewControls->addStretch(1);
    rightLayout->addLayout(previewControls);

    m_fileLabel = new QLabel(tr("No model loaded"), rightPane);
    m_fileLabel->setWordWrap(true);
    rightLayout->addWidget(m_fileLabel);

    m_viewportHost = new QWidget(rightPane);
    m_viewportHostLayout = new QVBoxLayout(m_viewportHost);
    m_viewportHostLayout->setContentsMargins(0, 0, 0, 0);
    m_viewportHostLayout->setSpacing(0);
    m_viewportPlaceholder = new QLabel(tr("Select a model to initialize the 3D preview."), m_viewportHost);
    m_viewportPlaceholder->setAlignment(Qt::AlignCenter);
    m_viewportPlaceholder->setWordWrap(true);
    m_viewportHostLayout->addWidget(m_viewportPlaceholder, 1);
    rightLayout->addWidget(m_viewportHost, 1);

    auto *detailsBox = new QGroupBox(tr("Details"), rightPane);
    auto *detailsLayout = new QVBoxLayout(detailsBox);
    m_detailsLabel = new QLabel(tr("Select a model to inspect details."), detailsBox);
    m_detailsLabel->setWordWrap(true);
    m_detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailsLayout->addWidget(m_detailsLabel);
    rightLayout->addWidget(detailsBox);

    auto *actionsRow = new QHBoxLayout();
    m_openSeparatePreviewButton = new QPushButton(tr("Open Separate Preview"), rightPane);
    connect(m_openSeparatePreviewButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentEntry)
            return;
        auto *dlg = new ModelPreview(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->loadModel(m_currentEntry->modelPath);
        dlg->show();
    });
    actionsRow->addWidget(m_openSeparatePreviewButton);

    m_openSourceIniButton = new QPushButton(tr("Open Source INI"), rightPane);
    connect(m_openSourceIniButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentEntry || m_currentEntry->sourceIniPath.isEmpty())
            return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentEntry->sourceIniPath));
    });
    actionsRow->addWidget(m_openSourceIniButton);

    m_openModelFileButton = new QPushButton(tr("Open Model File"), rightPane);
    connect(m_openModelFileButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentEntry)
            return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentEntry->modelPath));
    });
    actionsRow->addWidget(m_openModelFileButton);
    actionsRow->addStretch(1);
    rightLayout->addLayout(actionsRow);

    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 5);

    rebuildEntries();
    updateButtons();
}

const flatlas::infrastructure::ModelAssetEntry *ModelViewerPage::findEntryByModelPath(const QString &modelPath) const
{
    if (modelPath.trimmed().isEmpty())
        return nullptr;

    const QString normalizedTarget = normalizedModelLookupKey(modelPath);
    for (const auto &entry : m_entries) {
        if (normalizedModelLookupKey(entry.modelPath) == normalizedTarget)
            return &entry;
    }
    return nullptr;
}

bool ModelViewerPage::ensureViewport()
{
    if (m_viewport)
        return true;
    if (!m_viewportHost || !m_viewportHostLayout)
        return false;

    try {
        m_viewport = new ModelViewport3D(m_viewportHost);
    } catch (...) {
        m_viewport = nullptr;
    }

    if (!m_viewport) {
        if (m_viewportPlaceholder)
            m_viewportPlaceholder->setText(tr("The 3D preview could not be initialized."));
        QMessageBox::warning(this, tr("3D Model Viewer"), tr("The 3D preview could not be initialized."));
        return false;
    }

    if (m_viewportPlaceholder) {
        m_viewportPlaceholder->hide();
        m_viewportPlaceholder->deleteLater();
        m_viewportPlaceholder = nullptr;
    }

    m_viewportHostLayout->addWidget(m_viewport, 1);
    connect(m_boundsCheck, &QCheckBox::toggled, m_viewport, &ModelViewport3D::setBoundingBoxVisible);
    connect(m_wireframeCheck, &QCheckBox::toggled, m_viewport, &ModelViewport3D::setWireframeVisible);
    connect(m_meshCheck, &QCheckBox::toggled, m_viewport, &ModelViewport3D::setMeshVisible);
    connect(m_whiteBgCheck, &QCheckBox::toggled, m_viewport, &ModelViewport3D::setWhiteBackground);
    connect(m_viewport, &ModelViewport3D::modelLoaded, this,
            [this](const QString &filePath, bool success, const QString &message) {
        if (success)
            m_fileLabel->setText(tr("Loaded: %1").arg(filePath));
        else if (!message.isEmpty())
            m_fileLabel->setText(message);
        updateButtons();
    });

    m_viewport->setBoundingBoxVisible(m_boundsCheck->isChecked());
    m_viewport->setWireframeVisible(m_wireframeCheck->isChecked());
    m_viewport->setMeshVisible(m_meshCheck->isChecked());
    m_viewport->setWhiteBackground(m_whiteBgCheck->isChecked());
    return true;
}

void ModelViewerPage::rebuildEntries()
{
    m_entries = flatlas::infrastructure::ModelAssetScanner::scanCurrentContext();
    rebuildTree();
}

void ModelViewerPage::rebuildTree()
{
    const QString query = m_searchEdit->text().trimmed().toLower();
    const QString previousPath = m_currentEntry ? m_currentEntry->modelPath : QString();

    m_tree->clear();
    m_tree->blockSignals(true);

    QHash<QString, QTreeWidgetItem *> groups;
    int visibleCount = 0;
    QTreeWidgetItem *selectedItem = nullptr;

    for (const auto &entry : m_entries) {
        if (!query.isEmpty() && !entry.searchBlob().contains(query))
            continue;

        ++visibleCount;
        QTreeWidgetItem *group = groups.value(entry.categoryKey);
        if (!group) {
            group = new QTreeWidgetItem(m_tree, QStringList{entry.categoryLabel});
            group->setFirstColumnSpanned(true);
            group->setFlags(group->flags() & ~Qt::ItemIsSelectable);
            groups.insert(entry.categoryKey, group);
        }

        auto *item = new QTreeWidgetItem(group, QStringList{
            entry.titleText(),
            entry.archetype.isEmpty() ? entry.nickname : entry.archetype,
            entry.renderKind
        });
        item->setData(0, Qt::UserRole, entry.modelPath);
        if (entry.modelPath == previousPath)
            selectedItem = item;
    }

    for (auto *group : std::as_const(groups)) {
        group->setText(0, tr("%1 (%2)").arg(group->text(0)).arg(group->childCount()));
        group->setExpanded(true);
    }

    m_summaryLabel->setText(tr("%1 models").arg(visibleCount));

    if (selectedItem)
        m_tree->setCurrentItem(selectedItem);
    m_tree->blockSignals(false);
    setCurrentEntry(selectedItem ? findEntryByModelPath(selectedItem->data(0, Qt::UserRole).toString())
                                 : nullptr);
}

void ModelViewerPage::setCurrentEntry(const flatlas::infrastructure::ModelAssetEntry *entry)
{
    m_currentEntry = entry;
    updateDetails();
    updateButtons();
    if (!m_currentEntry) {
        if (m_viewport)
            m_viewport->clearModel();
        m_fileLabel->setText(tr("No model loaded"));
        return;
    }
    if (m_viewport)
        loadEntryIntoViewport(m_currentEntry);
}

void ModelViewerPage::loadEntryIntoViewport(const flatlas::infrastructure::ModelAssetEntry *entry)
{
    if (!entry)
        return;
    if (!ensureViewport())
        return;
    m_fileLabel->setText(tr("Initializing 3D preview..."));
    scheduleViewportLoad(entry->modelPath, true);
}

void ModelViewerPage::loadArbitraryModel()
{
    QString startDir = currentDataPath();
    if (startDir.isEmpty())
        startDir = QDir::homePath();

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Freelancer Model"),
        startDir,
        tr("Freelancer Models (*.cmp *.3db)"));
    if (filePath.isEmpty())
        return;

    m_currentEntry = nullptr;
    updateDetails();
    updateButtons();
    if (!ensureViewport())
        return;
    m_fileLabel->setText(tr("Initializing 3D preview..."));
    scheduleViewportLoad(filePath, false);
}

void ModelViewerPage::scheduleViewportLoad(const QString &modelPath, bool requireCurrentEntryMatch)
{
    m_pendingModelPath = modelPath;
    m_pendingLoadRequiresCurrentEntryMatch = requireCurrentEntryMatch;
    if (m_loadTimer)
        m_loadTimer->start(0);
}

void ModelViewerPage::executeScheduledViewportLoad()
{
    if (m_modelLoadInProgress || !m_viewport)
        return;

    const QString modelPath = m_pendingModelPath;
    const bool requireCurrentEntryMatch = m_pendingLoadRequiresCurrentEntryMatch;
    m_pendingModelPath.clear();
    m_pendingLoadRequiresCurrentEntryMatch = false;

    if (modelPath.trimmed().isEmpty())
        return;

    if (requireCurrentEntryMatch) {
        if (!m_currentEntry)
            return;
        if (normalizedModelLookupKey(m_currentEntry->modelPath) != normalizedModelLookupKey(modelPath))
            return;
    }

    if (m_viewport->hasModel() &&
        normalizedModelLookupKey(m_viewport->currentFilePath()) == normalizedModelLookupKey(modelPath)) {
        updateButtons();
        return;
    }

    m_modelLoadInProgress = true;
    QString error;
    const bool loaded = m_viewport->loadModelFile(modelPath, &error);
    m_modelLoadInProgress = false;

    if (!m_pendingModelPath.isEmpty()) {
        if (m_loadTimer)
            m_loadTimer->start(0);
    }

    if (!loaded && !error.isEmpty()) {
        m_fileLabel->setText(error);
        QMessageBox::warning(this, tr("3D Model Viewer"), error);
    }
}

void ModelViewerPage::updateButtons()
{
    const bool hasEntry = m_currentEntry != nullptr;
    m_resetCameraButton->setEnabled(m_viewport && m_viewport->hasModel());
    m_openSourceIniButton->setEnabled(hasEntry && !m_currentEntry->sourceIniPath.isEmpty());
    m_openModelFileButton->setEnabled(hasEntry);
    m_openSeparatePreviewButton->setEnabled(hasEntry);
}

void ModelViewerPage::updateDetails()
{
    if (!m_currentEntry) {
        m_detailsLabel->setText(tr("Select a model to inspect details."));
        return;
    }

    QStringList lines;
    lines << tr("Name: %1").arg(m_currentEntry->titleText());
    lines << tr("Nickname: %1").arg(m_currentEntry->nickname);
    lines << tr("Archetype: %1").arg(m_currentEntry->archetype);
    lines << tr("Category: %1").arg(m_currentEntry->categoryLabel);
    lines << tr("Render: %1").arg(m_currentEntry->renderKind);
    lines << tr("Model: %1").arg(m_currentEntry->relativeModelPath.isEmpty()
                                      ? m_currentEntry->modelPath
                                      : m_currentEntry->relativeModelPath);
    if (!m_currentEntry->sourceIniPath.isEmpty())
        lines << tr("Source INI: %1").arg(m_currentEntry->sourceIniPath);
    if (m_currentEntry->idsName > 0)
        lines << tr("ids_name: %1").arg(m_currentEntry->idsName);
    if (!m_currentEntry->typeValue.isEmpty())
        lines << tr("Type: %1").arg(m_currentEntry->typeValue);
    m_detailsLabel->setText(lines.join(QLatin1Char('\n')));
}

} // namespace flatlas::rendering
