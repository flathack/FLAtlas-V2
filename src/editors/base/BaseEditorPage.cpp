// editors/base/BaseEditorPage.cpp – Basis-Editor (Phase 10)

#include "BaseEditorPage.h"
#include "BasePersistence.h"
#include "BaseBuilder.h"
#include "RoomEditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QToolBar>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QSignalBlocker>

using namespace flatlas::domain;

namespace flatlas::editors {

BaseEditorPage::BaseEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

BaseEditorPage::~BaseEditorPage() = default;

// ─── UI Setup ────────────────────────────────────────────

void BaseEditorPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupToolBar();
    mainLayout->addWidget(m_toolBar);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left: Properties
    auto *propsWidget = new QWidget(this);
    auto *propsLayout = new QVBoxLayout(propsWidget);

    auto *propsGroup = new QGroupBox(tr("Base Properties"), propsWidget);
    auto *form = new QFormLayout(propsGroup);

    m_nicknameEdit = new QLineEdit(propsGroup);
    form->addRow(tr("Nickname:"), m_nicknameEdit);

    m_archetypeEdit = new QLineEdit(propsGroup);
    form->addRow(tr("Archetype:"), m_archetypeEdit);

    m_systemEdit = new QLineEdit(propsGroup);
    form->addRow(tr("System:"), m_systemEdit);

    m_idsNameSpin = new QSpinBox(propsGroup);
    m_idsNameSpin->setRange(0, 999999999);
    form->addRow(tr("IDS Name:"), m_idsNameSpin);

    m_idsInfoSpin = new QSpinBox(propsGroup);
    m_idsInfoSpin->setRange(0, 999999999);
    form->addRow(tr("IDS Info:"), m_idsInfoSpin);

    m_posEdit = new QLineEdit(propsGroup);
    m_posEdit->setPlaceholderText(tr("x, y, z"));
    form->addRow(tr("Position:"), m_posEdit);

    m_dockWithEdit = new QLineEdit(propsGroup);
    m_dockWithEdit->setPlaceholderText(tr("comma-separated nicknames"));
    form->addRow(tr("Dock With:"), m_dockWithEdit);

    propsLayout->addWidget(propsGroup);
    propsLayout->addStretch();

    // Right: Room editor
    m_roomEditor = new RoomEditor(this);
    connect(m_roomEditor, &RoomEditor::roomsChanged, this, [this]() {
        if (m_data && !m_loadingUi)
            markDirty();
    });

    m_splitter->addWidget(propsWidget);
    m_splitter->addWidget(m_roomEditor);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter);

    auto wireLineEdit = [this](QLineEdit *edit) {
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            if (!m_loadingUi)
                markDirty();
        });
    };
    wireLineEdit(m_nicknameEdit);
    wireLineEdit(m_archetypeEdit);
    wireLineEdit(m_systemEdit);
    wireLineEdit(m_posEdit);
    wireLineEdit(m_dockWithEdit);

    connect(m_idsNameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_loadingUi)
            markDirty();
    });
    connect(m_idsInfoSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_loadingUi)
            markDirty();
    });
}

void BaseEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setMovable(false);

    m_toolBar->addAction(tr("New Base..."), this, &BaseEditorPage::onNewBase);
    m_toolBar->addAction(tr("Apply Changes"), this, [this]() {
        applyToData();
        if (m_data)
            markDirty();
    });
}

// ─── Load / Save ─────────────────────────────────────────

bool BaseEditorPage::loadBase(const QString &filePath, const QString &baseNickname)
{
    auto data = BasePersistence::loadFromIni(filePath, baseNickname);
    if (!data) return false;

    m_data = std::move(data);
    m_filePath = filePath;

    populateFromData();
    setDirty(false);
    return true;
}

void BaseEditorPage::setBase(std::unique_ptr<BaseData> base)
{
    m_data = std::move(base);
    populateFromData();
    setDirty(false);
}

bool BaseEditorPage::save(const QString &filePath)
{
    if (!m_data) return false;
    applyToData();
    if (!BasePersistence::save(*m_data, filePath))
        return false;
    m_filePath = filePath;
    setDirty(false);
    return true;
}

BaseData *BaseEditorPage::data() const
{
    return m_data.get();
}

QString BaseEditorPage::baseNickname() const
{
    return m_data ? m_data->nickname : QString();
}

// ─── Data ↔ UI ──────────────────────────────────────────

void BaseEditorPage::populateFromData()
{
    if (!m_data) return;

    m_loadingUi = true;

    m_nicknameEdit->setText(m_data->nickname);
    m_archetypeEdit->setText(m_data->archetype);
    m_systemEdit->setText(m_data->system);
    m_idsNameSpin->setValue(m_data->idsName);
    m_idsInfoSpin->setValue(m_data->idsInfo);

    m_posEdit->setText(QStringLiteral("%1, %2, %3")
        .arg(m_data->position.x(), 0, 'f', 0)
        .arg(m_data->position.y(), 0, 'f', 0)
        .arg(m_data->position.z(), 0, 'f', 0));

    m_dockWithEdit->setText(m_data->dockWith.join(QStringLiteral(", ")));

    m_roomEditor->setBaseNickname(m_data->nickname);
    m_roomEditor->setRooms(m_data->rooms);
    m_loadingUi = false;
}

void BaseEditorPage::applyToData()
{
    if (!m_data) return;

    m_data->nickname = m_nicknameEdit->text().trimmed();
    m_data->archetype = m_archetypeEdit->text().trimmed();
    m_data->system = m_systemEdit->text().trimmed();
    m_data->idsName = m_idsNameSpin->value();
    m_data->idsInfo = m_idsInfoSpin->value();

    // Parse position
    auto parts = m_posEdit->text().split(',');
    if (parts.size() >= 3) {
        m_data->position = QVector3D(
            parts[0].trimmed().toFloat(),
            parts[1].trimmed().toFloat(),
            parts[2].trimmed().toFloat());
    }

    // Parse dock_with
    m_data->dockWith.clear();
    for (const auto &d : m_dockWithEdit->text().split(',')) {
        QString trimmed = d.trimmed();
        if (!trimmed.isEmpty())
            m_data->dockWith.append(trimmed);
    }

    m_data->rooms = m_roomEditor->rooms();
    m_roomEditor->setBaseNickname(m_data->nickname);
}

// ─── New Base (from template) ────────────────────────────

void BaseEditorPage::onNewBase()
{
    bool ok;
    QString nickname = QInputDialog::getText(this, tr("New Base"),
                                              tr("Base nickname:"),
                                              QLineEdit::Normal, QString(), &ok);
    if (!ok || nickname.trimmed().isEmpty()) return;
    nickname = nickname.trimmed();

    QStringList templates = BaseBuilder::templateNames();
    QString tmplName = QInputDialog::getItem(this, tr("Base Template"),
                                              tr("Select template:"),
                                              templates, 0, false, &ok);
    if (!ok) return;

    auto tmpl = BaseBuilder::templateFromName(tmplName);
    auto base = BaseBuilder::create(tmpl, nickname, QString());

    m_data = std::make_unique<BaseData>(std::move(base));
    populateFromData();
    setDirty(false);
}

void BaseEditorPage::markDirty()
{
    setDirty(true);
}

void BaseEditorPage::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        refreshTitle();
        return;
    }
    m_dirty = dirty;
    refreshTitle();
}

void BaseEditorPage::refreshTitle()
{
    if (!m_data)
        return;

    QString title = m_data->nickname.trimmed();
    if (title.isEmpty())
        title = tr("Base Editor");
    if (m_dirty)
        title += QLatin1Char('*');
    emit titleChanged(title);
}

} // namespace flatlas::editors
