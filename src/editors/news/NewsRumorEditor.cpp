#include "NewsRumorEditor.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/io/DllResources.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QCompleter>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStringListModel>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using flatlas::infrastructure::IdsDataService;
using flatlas::infrastructure::IdsDataset;
using flatlas::infrastructure::DllResources;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniEntry;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

namespace flatlas::editors {
namespace {

QString keyOf(const QString &value)
{
    return value.trimmed().toLower();
}

int toInt(const QString &value, int fallback = 0)
{
    bool ok = false;
    const int parsed = value.trimmed().toInt(&ok);
    return ok ? parsed : fallback;
}

QString csvList(const QStringList &values)
{
    return values.join(QStringLiteral(", "));
}

QString displayLabel(const QString &nickname, const QString &displayName)
{
    const QString cleanDisplay = displayName.trimmed();
    return cleanDisplay.isEmpty() ? nickname : cleanDisplay;
}

QStringList uniqueSortedList(QStringList values)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString &value : values) {
        const QString clean = value.trimmed();
        if (clean.isEmpty())
            continue;
        const QString key = clean.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        result.append(clean);
    }
    result.sort(Qt::CaseInsensitive);
    return result;
}

QString rankPart(const QString &rank, int index)
{
    const QStringList parts = rank.split(QLatin1Char(','));
    if (index < 0 || index >= parts.size())
        return {};
    return parts.at(index).trimmed();
}

QString joinedRank(const QString &from, const QString &to)
{
    const QString cleanFrom = from.trimmed();
    const QString cleanTo = to.trimmed();
    if (cleanFrom.isEmpty())
        return cleanTo;
    if (cleanTo.isEmpty())
        return cleanFrom;
    return cleanFrom + QLatin1Char(',') + cleanTo;
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QString htmlEscape(const QString &text)
{
    return text.toHtmlEscaped();
}

QString htmlList(const QStringList &items)
{
    if (items.isEmpty())
        return QStringLiteral("<p><i>%1</i></p>").arg(htmlEscape(QObject::tr("None")));
    QString html = QStringLiteral("<ul>");
    for (const QString &item : items)
        html += QStringLiteral("<li>%1</li>").arg(htmlEscape(item));
    html += QStringLiteral("</ul>");
    return html;
}

QString htmlSection(const QString &title, const QStringList &items)
{
    return QStringLiteral("<h3>%1</h3>%2").arg(htmlEscape(title), htmlList(items));
}

void setFirstValue(IniSection *section, const QString &key, const QString &value)
{
    for (IniEntry &entry : section->entries) {
        if (entry.first.compare(key, Qt::CaseInsensitive) == 0) {
            entry.second = value;
            return;
        }
    }
    section->entries.append({key, value});
}

void setFlagEntry(IniSection *section, const QString &key, bool enabled)
{
    for (int i = section->entries.size() - 1; i >= 0; --i) {
        if (section->entries.at(i).first.compare(key, Qt::CaseInsensitive) == 0)
            section->entries.removeAt(i);
    }
    if (enabled)
        section->entries.append({key, QString()});
}

void replaceRepeatedValues(IniSection *section, const QString &key, const QStringList &values)
{
    int insertAt = section->entries.size();
    for (int i = 0; i < section->entries.size(); ++i) {
        if (section->entries.at(i).first.compare(key, Qt::CaseInsensitive) == 0) {
            insertAt = i;
            break;
        }
    }
    for (int i = section->entries.size() - 1; i >= 0; --i) {
        if (section->entries.at(i).first.compare(key, Qt::CaseInsensitive) == 0)
            section->entries.removeAt(i);
    }
    int offset = 0;
    for (const QString &value : values) {
        const QString clean = value.trimmed();
        if (!clean.isEmpty())
            section->entries.insert(qMin(insertAt + offset++, static_cast<int>(section->entries.size())), {key, clean});
    }
}

QIcon newsIconForName(const QString &iconName)
{
    const QString key = iconName.trimmed().toLower();
    static QHash<QString, QIcon> cache;
    if (cache.contains(key))
        return cache.value(key);

    QColor accent(QStringLiteral("#6b7280"));
    QString glyph = key.left(1).toUpper();
    if (key == QStringLiteral("critical")) {
        accent = QColor(QStringLiteral("#d94848"));
        glyph = QStringLiteral("!");
    } else if (key == QStringLiteral("world")) {
        accent = QColor(QStringLiteral("#3b82f6"));
        glyph = QStringLiteral("W");
    } else if (key == QStringLiteral("system")) {
        accent = QColor(QStringLiteral("#14b8a6"));
        glyph = QStringLiteral("S");
    } else if (key == QStringLiteral("universe")) {
        accent = QColor(QStringLiteral("#8b5cf6"));
        glyph = QStringLiteral("U");
    } else if (key == QStringLiteral("faction")) {
        accent = QColor(QStringLiteral("#f59e0b"));
        glyph = QStringLiteral("F");
    }

    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(accent.darker(130), 1));
    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(2.5, 2.5, 19, 19), 4, 4);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, glyph);
    painter.end();

    const QIcon icon(pixmap);
    cache.insert(key, icon);
    return icon;
}

class DeleteNewsDialog final : public QDialog {
public:
    DeleteNewsDialog(const NewsEntry &entry, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_entry(entry)
    {
        setWindowTitle(tr("Delete News"));
        resize(720, 520);

        auto *layout = new QVBoxLayout(this);
        auto *intro = new QLabel(
            tr("Delete is blocked until Scan completes. The scan lists the news.ini section, base assignments and IDS references that will be removed with this news item."),
            this);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        m_reportView = new QTextBrowser(this);
        m_reportView->setOpenExternalLinks(false);
        layout->addWidget(m_reportView, 1);

        auto *buttons = new QDialogButtonBox(this);
        m_scanButton = buttons->addButton(tr("Scan"), QDialogButtonBox::ActionRole);
        m_deleteButton = buttons->addButton(tr("Delete News"), QDialogButtonBox::DestructiveRole);
        buttons->addButton(QDialogButtonBox::Cancel);
        m_deleteButton->setEnabled(false);
        layout->addWidget(buttons);

        connect(m_scanButton, &QPushButton::clicked, this, &DeleteNewsDialog::runScan);
        connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
            runScan();
            if (m_canDelete)
                accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        m_reportView->setHtml(QStringLiteral("<p><i>%1</i></p>").arg(htmlEscape(tr("No scan executed yet."))));
    }

private:
    void runScan()
    {
        m_scanCompleted = true;
        m_canDelete = !m_entry.removed;
        m_reportView->setHtml(formatReportHtml());
        m_deleteButton->setEnabled(m_scanCompleted && m_canDelete);
    }

    QString formatReportHtml() const
    {
        QString html;
        html += QStringLiteral("<html><body style=\"font-family:'Segoe UI';\">");
        html += QStringLiteral("<h2>%1</h2>").arg(htmlEscape(m_entry.headlineText.trimmed().isEmpty()
            ? tr("<IDS %1 missing>").arg(m_entry.headlineIds)
            : m_entry.headlineText.trimmed()));

        QStringList sectionLines;
        sectionLines.append(m_entry.sectionIndex >= 0
                                ? tr("%1 | [NewsItem] section %2").arg(m_entry.source, QString::number(m_entry.sectionIndex + 1))
                                : tr("Unsaved [NewsItem] entry"));
        sectionLines.append(tr("rank = %1").arg(m_entry.rank));
        sectionLines.append(tr("icon = %1").arg(m_entry.icon));
        sectionLines.append(tr("logo = %1").arg(m_entry.logo));
        html += htmlSection(tr("News Item To Remove"), sectionLines);

        QStringList baseLines;
        for (const QString &base : m_entry.bases)
            baseLines.append(tr("base = %1").arg(base));
        html += htmlSection(tr("Base Assignments To Remove"), baseLines);

        QStringList idsLines;
        if (m_entry.categoryIds > 0)
            idsLines.append(tr("category = %1").arg(m_entry.categoryIds));
        if (m_entry.headlineIds > 0)
            idsLines.append(tr("headline = %1").arg(m_entry.headlineIds));
        if (m_entry.textIds > 0)
            idsLines.append(tr("text = %1").arg(m_entry.textIds));
        html += htmlSection(tr("IDS References In This News Item"), idsLines);

        QStringList result;
        result.append(m_canDelete
                          ? tr("Scan successful. Delete is enabled.")
                          : tr("Scan blocked. Delete stays disabled."));
        result.append(tr("Save writes the removal to news.ini."));
        html += htmlSection(tr("Result"), result);
        html += QStringLiteral("</body></html>");
        return html;
    }

    NewsEntry m_entry;
    QTextBrowser *m_reportView = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    bool m_scanCompleted = false;
    bool m_canDelete = false;
};

} // namespace

NewsRumorEditor::NewsRumorEditor(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connect(&flatlas::core::EditingContext::instance(), &flatlas::core::EditingContext::contextChanged,
            this, [this]() { scheduleLoadFromContext(); });
    scheduleLoadFromContext();
}

void NewsRumorEditor::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    auto *toolbar = new QHBoxLayout();
    auto *reloadButton = new QPushButton(tr("Reload"), this);
    auto *openButton = new QPushButton(tr("Open File..."), this);
    auto *addButton = new QPushButton(tr("New News"), this);
    auto *removeButton = new QPushButton(tr("Delete News"), this);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setStyleSheet(QStringLiteral("QPushButton { background: #1f9d55; color: white; font-weight: bold; padding: 6px 18px; }"
                                               "QPushButton:disabled { background: #335344; color: #9aaba0; }"));
    toolbar->addWidget(reloadButton);
    toolbar->addWidget(openButton);
    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    toolbar->addWidget(separator);
    toolbar->addWidget(addButton);
    toolbar->addWidget(removeButton);
    toolbar->addStretch(1);
    toolbar->addWidget(m_saveButton);
    mainLayout->addLayout(toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter, 1);

    auto *basePane = new QWidget(splitter);
    auto *baseLayout = new QVBoxLayout(basePane);
    baseLayout->setContentsMargins(0, 0, 0, 0);
    baseLayout->addWidget(new QLabel(tr("Bases"), basePane));
    m_baseSearchEdit = new QLineEdit(basePane);
    m_baseSearchEdit->setPlaceholderText(tr("Search base, nickname, or system..."));
    m_baseSearchEdit->setClearButtonEnabled(true);
    baseLayout->addWidget(m_baseSearchEdit);
    m_baseTable = new QTableWidget(0, BaseColumnCount, basePane);
    m_baseTable->setHorizontalHeaderLabels({tr("Ingame Name"), tr("Nickname"), tr("System"), tr("News")});
    m_baseTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_baseTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_baseTable->setAlternatingRowColors(true);
    m_baseTable->horizontalHeader()->setStretchLastSection(false);
    m_baseTable->horizontalHeader()->setSectionResizeMode(BaseNameColumn, QHeaderView::Stretch);
    m_baseTable->horizontalHeader()->setSectionResizeMode(BaseNicknameColumn, QHeaderView::ResizeToContents);
    m_baseTable->horizontalHeader()->setSectionResizeMode(BaseSystemColumn, QHeaderView::ResizeToContents);
    m_baseTable->horizontalHeader()->setSectionResizeMode(BaseNewsCountColumn, QHeaderView::ResizeToContents);
    baseLayout->addWidget(m_baseTable, 1);
    splitter->addWidget(basePane);

    auto *newsPane = new QWidget(splitter);
    auto *newsLayout = new QVBoxLayout(newsPane);
    newsLayout->setContentsMargins(0, 0, 0, 0);
    newsLayout->addWidget(new QLabel(tr("News"), newsPane));
    auto *filterRow = new QHBoxLayout();
    m_scopeCombo = new QComboBox(newsPane);
    m_scopeCombo->addItem(tr("Selected Base"), QStringLiteral("base"));
    m_scopeCombo->addItem(tr("All News"), QStringLiteral("all"));
    m_scopeCombo->addItem(tr("Global News"), QStringLiteral("global"));
    m_missingIdsOnly = new QCheckBox(tr("Missing IDS"), newsPane);
    m_newsSearchEdit = new QLineEdit(newsPane);
    m_newsSearchEdit->setPlaceholderText(tr("Search headline, text, IDS, base, or rank..."));
    m_newsSearchEdit->setClearButtonEnabled(true);
    filterRow->addWidget(m_scopeCombo);
    filterRow->addWidget(m_missingIdsOnly);
    filterRow->addWidget(m_newsSearchEdit, 1);
    newsLayout->addLayout(filterRow);
    m_newsTable = new QTableWidget(0, NewsColumnCount, newsPane);
    m_newsTable->setHorizontalHeaderLabels({tr("Headline"), tr("Icon"), tr("Rank")});
    m_newsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_newsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_newsTable->setAlternatingRowColors(true);
    m_newsTable->setIconSize(QSize(20, 20));
    m_newsTable->horizontalHeader()->setSectionsMovable(true);
    m_newsTable->horizontalHeader()->setStretchLastSection(false);
    m_newsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_newsTable->setColumnWidth(NewsHeadlineColumn, 360);
    m_newsTable->setColumnWidth(NewsIconColumn, 110);
    m_newsTable->setColumnWidth(NewsRankColumn, 160);
    newsLayout->addWidget(m_newsTable, 1);
    splitter->addWidget(newsPane);

    auto *detailPane = new QWidget(splitter);
    auto *detailLayout = new QVBoxLayout(detailPane);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->addWidget(new QLabel(tr("Edit News"), detailPane));
    m_detailHintLabel = new QLabel(tr("Select a news item."), detailPane);
    m_detailHintLabel->setWordWrap(true);
    detailLayout->addWidget(m_detailHintLabel);
    m_headlineEdit = new QLineEdit(detailPane);
    m_headlineEdit->setPlaceholderText(tr("Headline"));
    detailLayout->addWidget(m_headlineEdit);
    m_bodyEdit = new QPlainTextEdit(detailPane);
    m_bodyEdit->setPlaceholderText(tr("News Text"));
    m_bodyEdit->setMinimumHeight(180);
    detailLayout->addWidget(m_bodyEdit, 1);
    m_basesEdit = new QPlainTextEdit(detailPane);
    m_basesEdit->setPlaceholderText(tr("Base nicknames, one per line or comma-separated. Empty = global news."));
    m_basesEdit->setMaximumHeight(100);
    detailLayout->addWidget(m_basesEdit);
    auto *baseAssignRow = new QHBoxLayout();
    auto *addSelectedBaseButton = new QPushButton(tr("Add Selected Base"), detailPane);
    auto *clearBasesButton = new QPushButton(tr("Clear Assignments"), detailPane);
    baseAssignRow->addWidget(addSelectedBaseButton);
    baseAssignRow->addWidget(clearBasesButton);
    baseAssignRow->addStretch(1);
    detailLayout->addLayout(baseAssignRow);

    auto *techBox = new QGroupBox(tr("Technical Fields"), detailPane);
    auto *techLayout = new QVBoxLayout(techBox);
    auto *idsRow = new QHBoxLayout();
    m_categorySpin = new QSpinBox(techBox);
    m_categorySpin->setRange(0, 999999999);
    m_headlineSpin = new QSpinBox(techBox);
    m_headlineSpin->setRange(0, 999999999);
    m_textSpin = new QSpinBox(techBox);
    m_textSpin->setRange(0, 999999999);
    idsRow->addWidget(new QLabel(tr("Category IDS:"), techBox));
    idsRow->addWidget(m_categorySpin);
    idsRow->addWidget(new QLabel(tr("Headline IDS:"), techBox));
    idsRow->addWidget(m_headlineSpin);
    idsRow->addWidget(new QLabel(tr("Text IDS:"), techBox));
    idsRow->addWidget(m_textSpin);
    techLayout->addLayout(idsRow);
    m_rankFromEdit = new QComboBox(techBox);
    m_rankToEdit = new QComboBox(techBox);
    m_iconEdit = new QComboBox(techBox);
    m_logoEdit = new QComboBox(techBox);
    for (QComboBox *combo : {m_rankFromEdit, m_rankToEdit, m_iconEdit, m_logoEdit}) {
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        if (combo->lineEdit())
            combo->lineEdit()->setClearButtonEnabled(true);
    }
    m_rankFromEdit->lineEdit()->setPlaceholderText(tr("Example: base_0_rank"));
    m_rankFromEdit->setToolTip(tr("Show this news starting at this rank."));
    m_rankToEdit->lineEdit()->setPlaceholderText(tr("Example: mission_end"));
    m_rankToEdit->setToolTip(tr("Show this news until this rank."));
    m_iconEdit->lineEdit()->setPlaceholderText(tr("Example: world"));
    m_iconEdit->setToolTip(tr("News icon name. Common values: critical, world, system, universe, faction."));
    m_logoEdit->lineEdit()->setPlaceholderText(tr("Example: news_manhattan"));
    m_logoEdit->setToolTip(tr("News vendor logo nickname, for example news_manhattan."));
    m_rankFromCompletionModel = new QStringListModel(this);
    m_rankToCompletionModel = new QStringListModel(this);
    m_iconCompletionModel = new QStringListModel(this);
    m_logoCompletionModel = new QStringListModel(this);
    auto *rankFromCompleter = new QCompleter(m_rankFromCompletionModel, this);
    auto *rankToCompleter = new QCompleter(m_rankToCompletionModel, this);
    auto *iconCompleter = new QCompleter(m_iconCompletionModel, this);
    auto *logoCompleter = new QCompleter(m_logoCompletionModel, this);
    for (QCompleter *completer : {rankFromCompleter, rankToCompleter, iconCompleter, logoCompleter}) {
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
    }
    m_rankFromEdit->setModel(m_rankFromCompletionModel);
    m_rankToEdit->setModel(m_rankToCompletionModel);
    m_iconEdit->setModel(m_iconCompletionModel);
    m_logoEdit->setModel(m_logoCompletionModel);
    m_rankFromEdit->setCompleter(rankFromCompleter);
    m_rankToEdit->setCompleter(rankToCompleter);
    m_iconEdit->setCompleter(iconCompleter);
    m_logoEdit->setCompleter(logoCompleter);
    updateTechnicalFieldCompletions();
    m_autoselectCheck = new QCheckBox(tr("Autoselect"), techBox);
    auto *rankRow = new QHBoxLayout();
    rankRow->addWidget(new QLabel(tr("Show From:"), techBox));
    rankRow->addWidget(m_rankFromEdit, 1);
    rankRow->addWidget(new QLabel(tr("Show Until:"), techBox));
    rankRow->addWidget(m_rankToEdit, 1);
    techLayout->addLayout(rankRow);
    techLayout->addWidget(new QLabel(tr("Icon:"), techBox));
    techLayout->addWidget(m_iconEdit);
    techLayout->addWidget(new QLabel(tr("Logo:"), techBox));
    techLayout->addWidget(m_logoEdit);
    techLayout->addWidget(m_autoselectCheck);
    detailLayout->addWidget(techBox);
    splitter->addWidget(detailPane);
    splitter->setSizes({300, 760, 520});

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(5);
    mainLayout->addWidget(m_progressBar);

    connect(reloadButton, &QPushButton::clicked, this, &NewsRumorEditor::scheduleLoadFromContext);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open News File"), QString(), tr("INI Files (*.ini);;All Files (*)"));
        if (!path.isEmpty())
            loadFromFile(path);
    });
    connect(addButton, &QPushButton::clicked, this, &NewsRumorEditor::addNews);
    connect(removeButton, &QPushButton::clicked, this, &NewsRumorEditor::removeSelectedNews);
    connect(m_saveButton, &QPushButton::clicked, this, [this]() { save(); });
    connect(addSelectedBaseButton, &QPushButton::clicked, this, [this]() {
        const int row = m_baseTable ? m_baseTable->currentRow() : -1;
        if (row <= 0 || !m_baseTable->item(row, BaseNameColumn))
            return;
        const int idx = m_baseTable->item(row, BaseNameColumn)->data(Qt::UserRole).toInt();
        if (idx <= 0 || idx >= m_bases.size())
            return;
        QStringList bases = detailBases();
        const QString nickname = m_bases.at(idx).nickname;
        bool exists = false;
        for (const QString &base : bases) {
            if (base.compare(nickname, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            bases.append(nickname);
            m_basesEdit->setPlainText(bases.join(QLatin1Char('\n')));
            setDirty(true);
        }
    });
    connect(clearBasesButton, &QPushButton::clicked, this, [this]() {
        m_basesEdit->clear();
        setDirty(true);
    });
    connect(m_baseSearchEdit, &QLineEdit::textChanged, this, &NewsRumorEditor::refreshFilters);
    connect(m_newsSearchEdit, &QLineEdit::textChanged, this, &NewsRumorEditor::refreshFilters);
    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, &NewsRumorEditor::refreshFilters);
    connect(m_missingIdsOnly, &QCheckBox::toggled, this, &NewsRumorEditor::refreshFilters);
    connect(m_baseTable, &QTableWidget::currentCellChanged, this, &NewsRumorEditor::onBaseSelectionChanged);
    connect(m_newsTable, &QTableWidget::currentCellChanged, this, &NewsRumorEditor::onNewsSelectionChanged);

    auto markDetailDirty = [this]() {
        if (m_populating || m_currentEntryIndex < 0)
            return;
        setDirty(true);
    };
    connect(m_headlineEdit, &QLineEdit::textEdited, this, markDetailDirty);
    connect(m_bodyEdit, &QPlainTextEdit::textChanged, this, markDetailDirty);
    connect(m_basesEdit, &QPlainTextEdit::textChanged, this, markDetailDirty);
    connect(m_categorySpin, &QSpinBox::valueChanged, this, markDetailDirty);
    connect(m_headlineSpin, &QSpinBox::valueChanged, this, markDetailDirty);
    connect(m_textSpin, &QSpinBox::valueChanged, this, markDetailDirty);
    connect(m_rankFromEdit, &QComboBox::currentTextChanged, this, markDetailDirty);
    connect(m_rankToEdit, &QComboBox::currentTextChanged, this, markDetailDirty);
    connect(m_iconEdit, &QComboBox::currentTextChanged, this, markDetailDirty);
    connect(m_logoEdit, &QComboBox::currentTextChanged, this, markDetailDirty);
    connect(m_autoselectCheck, &QCheckBox::toggled, this, markDetailDirty);
}

void NewsRumorEditor::scheduleLoadFromContext()
{
    if (m_statusLabel)
        m_statusLabel->setText(tr("Loading news data..."));
    QTimer::singleShot(0, this, &NewsRumorEditor::loadFromContext);
}

void NewsRumorEditor::loadFromContext()
{
    QString error;
    reportLoadingProgress(0, tr("News Editor: Preparing data..."));
    if (!loadWorkspace(flatlas::core::EditingContext::instance().primaryGamePath(), &error)) {
        clearData();
        m_statusLabel->setText(error.isEmpty() ? tr("No active mod installation.") : error);
        reportLoadingProgress(100, error.isEmpty() ? tr("News Editor: no active mod installation") : error);
    }
}

void NewsRumorEditor::reportLoadingProgress(int percent, const QString &message)
{
    setOperationProgress(percent, message);
}

void NewsRumorEditor::setOperationProgress(int percent, const QString &message)
{
    const int clamped = std::clamp(percent, 0, 100);
    emit loadingProgressChanged(clamped, message);
    if (m_progressBar) {
        m_progressBar->setValue(clamped);
        m_progressBar->update();
    }
    if (m_statusLabel)
        m_statusLabel->setText(message);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

bool NewsRumorEditor::loadWorkspace(const QString &gameRoot, QString *errorMessage)
{
    reportLoadingProgress(5, tr("News Editor: Checking context..."));
    const QString resolvedGameRoot = gameRoot.trimmed();
    if (resolvedGameRoot.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Please first select an installation for editing in Mod Manager.");
        return false;
    }

    reportLoadingProgress(15, tr("News Editor: Searching paths..."));
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(resolvedGameRoot, QStringLiteral("DATA"));
    const QString newsPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS/news.ini"));
    if (dataDir.isEmpty() || newsPath.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("DATA/MISSIONS/news.ini was not found in the active mod.");
        return false;
    }

    clearData();
    m_gameRoot = resolvedGameRoot;
    m_newsPath = newsPath;
    reportLoadingProgress(30, tr("News Editor: Loading IDS texts..."));
    loadIds(resolvedGameRoot);
    reportLoadingProgress(50, tr("News Editor: Loading bases..."));
    loadBases(dataDir);
    reportLoadingProgress(70, tr("News Editor: Reading news.ini..."));
    loadNewsFile(newsPath);
    reportLoadingProgress(88, tr("News Editor: Filling tables..."));
    rebuildBaseCounts();
    populateBaseTable();
    populateNewsTable();
    setDirty(false);
    emit titleChanged(tr("News Editor"));
    refreshStatus();
    reportLoadingProgress(100, tr("News Editor: %1 entries loaded").arg(m_entries.size()));
    return true;
}

void NewsRumorEditor::clearData()
{
    m_newsDoc.clear();
    m_entries.clear();
    m_bases.clear();
    m_baseIndexByKey.clear();
    m_idsTextByNumber.clear();
    m_currentEntryIndex = -1;
    m_gameRoot.clear();
    m_newsPath.clear();
    if (m_baseTable)
        m_baseTable->setRowCount(0);
    if (m_newsTable)
        m_newsTable->setRowCount(0);
}

void NewsRumorEditor::loadIds(const QString &gameRoot)
{
    const IdsDataset dataset = IdsDataService::loadFromGameRoot(gameRoot);
    for (const auto &entry : dataset.entries) {
        QString value = entry.hasStringValue ? entry.stringValue : entry.plainText;
        if (!value.trimmed().isEmpty()) {
            m_idsTextByNumber.insert(QString::number(entry.globalId), value.trimmed());
            if (entry.localId > 0 && !m_idsTextByNumber.contains(QString::number(entry.localId)))
                m_idsTextByNumber.insert(QString::number(entry.localId), value.trimmed());
        }
    }

    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    const QString resourcesDllPath = flatlas::core::PathUtils::ciResolvePath(exeDir, QStringLiteral("resources.dll"));
    if (!resourcesDllPath.isEmpty()) {
        const auto localStrings = DllResources::loadStrings(resourcesDllPath, 0, 65535);
        for (auto it = localStrings.constBegin(); it != localStrings.constEnd(); ++it) {
            const QString key = QString::number(it.key());
            if (!it.value().trimmed().isEmpty())
                m_idsTextByNumber.insert(key, it.value().trimmed());
        }
    }
}

void NewsRumorEditor::loadBases(const QString &dataDir)
{
    const QString universePath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    const IniDocument doc = IniParser::parseFile(universePath);
    NewsBaseRecord all;
    all.nickname = QStringLiteral("*");
    all.displayName = tr("All Bases");
    all.system = QStringLiteral("-");
    all.searchBlob = QStringLiteral("alle bases all");
    m_bases.append(all);
    m_baseIndexByKey.insert(keyOf(all.nickname), 0);

    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) != 0)
            continue;
        NewsBaseRecord base;
        base.nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (base.nickname.isEmpty())
            continue;
        base.system = section.value(QStringLiteral("system")).trimmed();
        const int strid = toInt(section.value(QStringLiteral("strid_name")));
        base.displayName = displayLabel(base.nickname, resolvedIdsText(strid));
        base.searchBlob = QStringLiteral("%1 %2 %3").arg(base.nickname, base.displayName, base.system).toLower();
        m_baseIndexByKey.insert(keyOf(base.nickname), m_bases.size());
        m_bases.append(base);
    }
}

void NewsRumorEditor::loadNewsFile(const QString &newsPath)
{
    m_newsDoc = IniParser::parseFile(newsPath);
    m_entries.clear();
    for (int i = 0; i < m_newsDoc.size(); ++i) {
        if (m_newsDoc.at(i).name.compare(QStringLiteral("NewsItem"), Qt::CaseInsensitive) == 0)
            appendEntryFromSection(m_newsDoc.at(i), i, newsPath);
    }
    updateTechnicalFieldCompletions();
}

void NewsRumorEditor::appendEntryFromSection(const IniSection &section, int sectionIndex, const QString &source)
{
    NewsEntry entry;
    entry.type = NewsEntry::News;
    entry.source = source;
    entry.sectionIndex = sectionIndex;
    entry.categoryIds = toInt(section.value(QStringLiteral("category")));
    entry.headlineIds = toInt(section.value(QStringLiteral("headline")));
    entry.textIds = toInt(section.value(QStringLiteral("text")));
    entry.ids = entry.textIds;
    entry.categoryText = resolvedIdsText(entry.categoryIds);
    entry.headlineText = resolvedIdsText(entry.headlineIds);
    entry.bodyText = resolvedIdsText(entry.textIds);
    entry.originalHeadlineText = entry.headlineText;
    entry.originalBodyText = entry.bodyText;
    entry.text = entry.bodyText;
    entry.bases = section.values(QStringLiteral("base"));
    entry.rank = csvList(section.values(QStringLiteral("rank")));
    entry.icon = section.value(QStringLiteral("icon"));
    entry.logo = section.value(QStringLiteral("logo"));
    for (const IniEntry &iniEntry : section.entries) {
        if (iniEntry.first.compare(QStringLiteral("autoselect"), Qt::CaseInsensitive) == 0) {
            entry.autoselect = true;
            break;
        }
    }
    entry.searchBlob = QStringLiteral("%1 %2 %3 %4 %5 %6 %7")
        .arg(entry.headlineText, entry.bodyText, QString::number(entry.headlineIds), QString::number(entry.textIds),
             csvList(entry.bases), entry.rank, entry.icon)
        .toLower();
    m_entries.append(entry);
}

void NewsRumorEditor::rebuildBaseCounts()
{
    for (NewsBaseRecord &base : m_bases)
        base.newsCount = 0;
    if (!m_bases.isEmpty())
        m_bases[0].newsCount = m_entries.size();
    for (const NewsEntry &entry : m_entries) {
        if (entry.removed)
            continue;
        for (const QString &baseNick : entry.bases) {
            const int idx = m_baseIndexByKey.value(keyOf(baseNick), -1);
            if (idx > 0 && idx < m_bases.size())
                ++m_bases[idx].newsCount;
        }
    }
}

void NewsRumorEditor::populateBaseTable()
{
    QSignalBlocker blocker(m_baseTable);
    m_baseTable->setRowCount(m_bases.size());
    for (int row = 0; row < m_bases.size(); ++row) {
        const NewsBaseRecord &base = m_bases.at(row);
        auto *name = readOnlyItem(base.displayName);
        name->setData(Qt::UserRole, row);
        m_baseTable->setItem(row, BaseNameColumn, name);
        m_baseTable->setItem(row, BaseNicknameColumn, readOnlyItem(base.nickname == QStringLiteral("*") ? QStringLiteral("*") : base.nickname));
        m_baseTable->setItem(row, BaseSystemColumn, readOnlyItem(base.system));
        m_baseTable->setItem(row, BaseNewsCountColumn, readOnlyItem(QString::number(base.newsCount)));
    }
    if (m_baseTable->rowCount() > 0)
        m_baseTable->setCurrentCell(0, 0);
    refreshFilters();
}

void NewsRumorEditor::populateNewsTable()
{
    QSignalBlocker blocker(m_newsTable);
    m_newsTable->setRowCount(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
    const NewsEntry &entry = m_entries.at(row);
    auto *headline = readOnlyItem(clippedTableText(entry.headlineText.isEmpty()
        ? tr("<IDS %1 missing>").arg(entry.headlineIds)
        : entry.headlineText, 90));
        headline->setData(Qt::UserRole, row);
        headline->setToolTip(entry.headlineText);
        m_newsTable->setItem(row, NewsHeadlineColumn, headline);
        auto *iconItem = readOnlyItem(entry.icon);
        iconItem->setIcon(newsIconForName(entry.icon));
        iconItem->setToolTip(entry.icon);
        m_newsTable->setItem(row, NewsIconColumn, iconItem);
        m_newsTable->setItem(row, NewsRankColumn, readOnlyItem(entry.rank));
    }
    refreshFilters();
}

void NewsRumorEditor::refreshFilters()
{
    if (!m_baseTable || !m_newsTable)
        return;

    const QString baseNeedle = m_baseSearchEdit->text().trimmed().toLower();
    for (int row = 0; row < m_baseTable->rowCount(); ++row) {
        const int idx = m_baseTable->item(row, BaseNameColumn)->data(Qt::UserRole).toInt();
        const bool visible = baseNeedle.isEmpty() || (idx >= 0 && idx < m_bases.size() && m_bases.at(idx).searchBlob.contains(baseNeedle));
        m_baseTable->setRowHidden(row, !visible);
    }

    const int baseRow = m_baseTable->currentRow();
    const int baseIndex = baseRow >= 0 && m_baseTable->item(baseRow, BaseNameColumn)
        ? m_baseTable->item(baseRow, BaseNameColumn)->data(Qt::UserRole).toInt()
        : 0;
    const QString selectedBase = baseIndex > 0 && baseIndex < m_bases.size() ? keyOf(m_bases.at(baseIndex).nickname) : QStringLiteral("*");
    const QString scope = m_scopeCombo->currentData().toString();
    const QString newsNeedle = m_newsSearchEdit->text().trimmed().toLower();

    int visibleCount = 0;
    for (int row = 0; row < m_newsTable->rowCount(); ++row) {
        if (row < 0 || row >= m_entries.size())
            continue;
        const NewsEntry &entry = m_entries.at(row);
        bool visible = !entry.removed;
        const bool globalNews = entry.bases.isEmpty();
        if (visible && scope == QStringLiteral("base") && selectedBase != QStringLiteral("*")) {
            visible = false;
            for (const QString &base : entry.bases) {
                if (keyOf(base) == selectedBase) {
                    visible = true;
                    break;
                }
            }
        } else if (visible && scope == QStringLiteral("global")) {
            visible = globalNews;
        }
        if (visible && m_missingIdsOnly->isChecked())
            visible = entry.headlineText.isEmpty() || entry.bodyText.isEmpty();
        if (visible && !newsNeedle.isEmpty())
            visible = entry.searchBlob.contains(newsNeedle);
        m_newsTable->setRowHidden(row, !visible);
        if (visible)
            ++visibleCount;
    }
    m_statusLabel->setText(tr("Shown: %1 of %2 news | Bases: %3 | File: %4")
                               .arg(visibleCount)
                               .arg(m_entries.size())
                               .arg(qMax(0, static_cast<int>(m_bases.size()) - 1))
                               .arg(m_newsPath));
}

void NewsRumorEditor::refreshStatus()
{
    refreshFilters();
    m_saveButton->setEnabled(!m_newsPath.isEmpty());
}

void NewsRumorEditor::updateTechnicalFieldCompletions()
{
    QStringList rankFromValues{QStringLiteral("base_0_rank")};
    QStringList rankToValues{QStringLiteral("mission_end")};
    QStringList icons{
        QStringLiteral("critical"),
        QStringLiteral("world"),
        QStringLiteral("system"),
        QStringLiteral("universe"),
        QStringLiteral("faction")
    };
    QStringList logos{
        QStringLiteral("news_manhattan")
    };

    for (const NewsEntry &entry : m_entries) {
        const QString from = rankPart(entry.rank, 0);
        const QString to = rankPart(entry.rank, 1);
        if (!from.isEmpty())
            rankFromValues.append(from);
        if (!to.isEmpty())
            rankToValues.append(to);
        icons.append(entry.icon);
        logos.append(entry.logo);
    }

    const QString currentRankFrom = m_rankFromEdit ? m_rankFromEdit->currentText() : QString();
    const QString currentRankTo = m_rankToEdit ? m_rankToEdit->currentText() : QString();
    const QString currentIcon = m_iconEdit ? m_iconEdit->currentText() : QString();
    const QString currentLogo = m_logoEdit ? m_logoEdit->currentText() : QString();

    if (m_rankFromCompletionModel)
        m_rankFromCompletionModel->setStringList(uniqueSortedList(rankFromValues));
    if (m_rankToCompletionModel)
        m_rankToCompletionModel->setStringList(uniqueSortedList(rankToValues));
    if (m_iconCompletionModel)
        m_iconCompletionModel->setStringList(uniqueSortedList(icons));
    if (m_logoCompletionModel)
        m_logoCompletionModel->setStringList(uniqueSortedList(logos));
    if (m_rankFromEdit)
        m_rankFromEdit->setCurrentText(currentRankFrom);
    if (m_rankToEdit)
        m_rankToEdit->setCurrentText(currentRankTo);
    if (m_iconEdit)
        m_iconEdit->setCurrentText(currentIcon);
    if (m_logoEdit)
        m_logoEdit->setCurrentText(currentLogo);
}

void NewsRumorEditor::onBaseSelectionChanged()
{
    refreshFilters();
}

void NewsRumorEditor::onNewsSelectionChanged()
{
    if (m_populating)
        return;
    const int idx = selectedEntryIndex();
    m_changingNewsSelection = true;
    applyDetailToCurrentEntry();
    m_changingNewsSelection = false;
    showEntryInDetail(idx);
    if (m_deferredTableRefresh) {
        m_deferredTableRefresh = false;
        QTimer::singleShot(0, this, [this]() {
            if (m_populating)
                return;
            const int entryIndex = m_currentEntryIndex;
            rebuildBaseCounts();
            populateBaseTable();
            populateNewsTable();
            if (entryIndex >= 0 && entryIndex < m_entries.size()) {
                QSignalBlocker blocker(m_newsTable);
                for (int row = 0; row < m_newsTable->rowCount(); ++row) {
                    auto *item = m_newsTable->item(row, NewsHeadlineColumn);
                    if (item && item->data(Qt::UserRole).toInt() == entryIndex) {
                        m_newsTable->setCurrentCell(row, NewsHeadlineColumn);
                        break;
                    }
                }
            }
        });
    }
}

void NewsRumorEditor::showEntryInDetail(int entryIndex)
{
    m_populating = true;
    m_currentEntryIndex = entryIndex;
    if (entryIndex < 0 || entryIndex >= m_entries.size()) {
        m_headlineEdit->clear();
        m_bodyEdit->clear();
        m_basesEdit->clear();
        m_categorySpin->setValue(0);
        m_headlineSpin->setValue(0);
        m_textSpin->setValue(0);
        m_rankFromEdit->setCurrentText(QString());
        m_rankToEdit->setCurrentText(QString());
        m_iconEdit->setCurrentText(QString());
        m_logoEdit->setCurrentText(QString());
        m_autoselectCheck->setChecked(false);
        m_detailHintLabel->setText(tr("Select a news item."));
        m_populating = false;
        return;
    }

    const NewsEntry &entry = m_entries.at(entryIndex);
    m_headlineEdit->setText(entry.headlineText);
    m_bodyEdit->setPlainText(entry.bodyText);
    m_basesEdit->setPlainText(entry.bases.join(QLatin1Char('\n')));
    m_categorySpin->setValue(entry.categoryIds);
    m_headlineSpin->setValue(entry.headlineIds);
    m_textSpin->setValue(entry.textIds);
    m_rankFromEdit->setCurrentText(rankPart(entry.rank, 0));
    m_rankToEdit->setCurrentText(rankPart(entry.rank, 1));
    m_iconEdit->setCurrentText(entry.icon);
    m_logoEdit->setCurrentText(entry.logo);
    m_autoselectCheck->setChecked(entry.autoselect);
    m_detailHintLabel->setText(tr("Headline IDS %1 | Text IDS %2 | %3 base assignments")
                                   .arg(entry.headlineIds)
                                   .arg(entry.textIds)
                                   .arg(entry.bases.size()));
    const QStringList invalid = invalidBases(entry.bases);
    if (!invalid.isEmpty()) {
        m_detailHintLabel->setText(m_detailHintLabel->text() + tr(" | Missing: %1").arg(invalid.join(QStringLiteral(", "))));
    }
    m_populating = false;
}

bool NewsRumorEditor::applyDetailToCurrentEntry()
{
    if (m_currentEntryIndex < 0 || m_currentEntryIndex >= m_entries.size())
        return true;
    NewsEntry &entry = m_entries[m_currentEntryIndex];
    const QString headline = m_headlineEdit->text().trimmed();
    const QString body = m_bodyEdit->toPlainText().trimmed();
    const QStringList bases = detailBases();
    const QString rank = joinedRank(m_rankFromEdit->currentText(), m_rankToEdit->currentText());
    const bool changed = entry.headlineText != headline ||
        entry.bodyText != body ||
        entry.bases != bases ||
        entry.categoryIds != m_categorySpin->value() ||
        entry.headlineIds != m_headlineSpin->value() ||
        entry.textIds != m_textSpin->value() ||
        entry.rank != rank ||
        entry.icon != m_iconEdit->currentText().trimmed() ||
        entry.logo != m_logoEdit->currentText().trimmed() ||
        entry.autoselect != m_autoselectCheck->isChecked();
    if (!changed)
        return true;
    if (!m_saving && !m_changingNewsSelection)
        setOperationProgress(10, tr("Updating news item..."));
    const bool headlineChanged = entry.headlineText != headline;
    const bool bodyChanged = entry.bodyText != body;
    entry.headlineText = headline;
    entry.bodyText = body;
    entry.text = body;
    entry.headlineTextDirty = entry.headlineTextDirty || headlineChanged;
    entry.bodyTextDirty = entry.bodyTextDirty || bodyChanged;
    entry.bases = bases;
    entry.categoryIds = m_categorySpin->value();
    entry.headlineIds = m_headlineSpin->value();
    entry.textIds = m_textSpin->value();
    entry.ids = entry.textIds;
    entry.rank = rank;
    entry.icon = m_iconEdit->currentText().trimmed();
    entry.logo = m_logoEdit->currentText().trimmed();
    entry.autoselect = m_autoselectCheck->isChecked();
    entry.modified = true;
    entry.searchBlob = QStringLiteral("%1 %2 %3 %4 %5 %6 %7")
        .arg(entry.headlineText, entry.bodyText, QString::number(entry.headlineIds), QString::number(entry.textIds),
             csvList(entry.bases), entry.rank, entry.icon)
        .toLower();
    if (m_changingNewsSelection) {
        m_deferredTableRefresh = true;
        setDirty(true);
    } else if (!m_saving) {
        setOperationProgress(55, tr("Refreshing news list..."));
        rebuildBaseCounts();
        populateBaseTable();
        populateNewsTable();
        setDirty(true);
        setOperationProgress(100, tr("News item updated."));
    }
    return true;
}

bool NewsRumorEditor::save()
{
    const int savedEntryIndex = m_currentEntryIndex;
    setOperationProgress(0, tr("Preparing news save..."));
    m_saving = true;
    applyDetailToCurrentEntry();
    m_saving = false;
    if (m_newsPath.isEmpty()) {
        QMessageBox::warning(this, tr("Save News"), tr("No news.ini loaded."));
        return false;
    }

    bool needsIdsDataset = false;
    for (const NewsEntry &entry : m_entries) {
        if (entry.removed || !entry.modified)
            continue;
        const bool needsHeadlineWrite = entry.sectionIndex < 0 || entry.headlineIds <= 0 ||
            (entry.headlineTextDirty && entry.headlineText != entry.originalHeadlineText);
        const bool needsBodyWrite = entry.sectionIndex < 0 || entry.textIds <= 0 ||
            (entry.bodyTextDirty && entry.bodyText != entry.originalBodyText);
        if (needsHeadlineWrite || needsBodyWrite) {
            needsIdsDataset = true;
            break;
        }
    }

    IdsDataset idsDataset;
    QString targetDll;
    if (needsIdsDataset) {
        setOperationProgress(15, tr("Preparing IDS data..."));
        idsDataset = IdsDataService::loadFromGameRoot(m_gameRoot);
        targetDll = IdsDataService::defaultCreationDllName(idsDataset);
        if (targetDll.trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("No target DLL found for IDS texts."), tr("No target DLL found for IDS texts."));
            return false;
        }
    }

    int processedEntries = 0;
    int modifiedEntries = 0;
    for (const NewsEntry &entry : m_entries) {
        if (!entry.removed && entry.modified)
            ++modifiedEntries;
    }
    for (NewsEntry &entry : m_entries) {
        if (entry.removed || !entry.modified)
            continue;
        ++processedEntries;
        const QStringList invalid = invalidBases(entry.bases);
        if (!invalid.isEmpty()) {
            QMessageBox::warning(this,
                                 tr("Save News"),
                                 tr("This news item contains base assignments that do not exist in universe.ini:\n\n%1")
                                     .arg(invalid.join(QLatin1Char('\n'))));
            return false;
        }
        QString error;
        const bool needsHeadlineWrite = entry.sectionIndex < 0 || entry.headlineIds <= 0 ||
            (entry.headlineTextDirty && entry.headlineText != entry.originalHeadlineText);
        const bool needsBodyWrite = entry.sectionIndex < 0 || entry.textIds <= 0 ||
            (entry.bodyTextDirty && entry.bodyText != entry.originalBodyText);
        if (needsHeadlineWrite) {
            setOperationProgress(25 + (modifiedEntries > 0 ? (processedEntries - 1) * 35 / modifiedEntries : 0),
                                 tr("Saving headline IDS %1...").arg(entry.headlineIds));
        }
        if (needsHeadlineWrite && !saveIdsText(idsDataset, targetDll, entry.headlineIds, entry.headlineText, &entry.headlineIds, &error)) {
            QMessageBox::warning(this, tr("Save News"), tr("Headline could not be saved: %1").arg(error));
            return false;
        }
        if (needsBodyWrite) {
            setOperationProgress(35 + (modifiedEntries > 0 ? (processedEntries - 1) * 35 / modifiedEntries : 0),
                                 tr("Saving text IDS %1...").arg(entry.textIds));
        }
        if (needsBodyWrite && !saveIdsText(idsDataset, targetDll, entry.textIds, entry.bodyText, &entry.textIds, &error)) {
            QMessageBox::warning(this, tr("Save News"), tr("Text could not be saved: %1").arg(error));
            return false;
        }
        if (entry.categoryIds <= 0)
            entry.categoryIds = entry.headlineIds;
    }

    QString error;
    setOperationProgress(75, tr("Saving news.ini..."));
    if (!writeNewsFile(&error)) {
        QMessageBox::warning(this, tr("Save News"), error);
        return false;
    }

    setOperationProgress(88, tr("Reloading news list..."));
    loadNewsFile(m_newsPath);
    rebuildBaseCounts();
    m_currentEntryIndex = savedEntryIndex >= 0 && savedEntryIndex < m_entries.size() ? savedEntryIndex : -1;
    populateBaseTable();
    populateNewsTable();
    if (m_currentEntryIndex >= 0) {
        QSignalBlocker blocker(m_newsTable);
        m_newsTable->setCurrentCell(m_currentEntryIndex, NewsHeadlineColumn);
    }
    showEntryInDetail(m_currentEntryIndex);
    setDirty(false);
    setOperationProgress(100, tr("Saved: %1").arg(m_newsPath));
    return true;
}

bool NewsRumorEditor::saveIdsText(int currentId, const QString &text, int *outId, QString *errorMessage)
{
    if (text.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Text is empty.");
        return false;
    }
    const IdsDataset dataset = IdsDataService::loadFromGameRoot(m_gameRoot);
    const QString targetDll = IdsDataService::defaultCreationDllName(dataset);
    if (targetDll.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("No target DLL found for IDS texts.");
        return false;
    }
    int newId = currentId;
    if (!IdsDataService::writeStringEntry(dataset, targetDll, currentId, text, &newId, errorMessage))
        return false;
    if (outId)
        *outId = newId;
    m_idsTextByNumber.insert(QString::number(newId), text);
    return true;
}

bool NewsRumorEditor::saveIdsText(const IdsDataset &dataset,
                                  const QString &targetDll,
                                  int currentId,
                                  const QString &text,
                                  int *outId,
                                  QString *errorMessage)
{
    if (text.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Text is empty.");
        return false;
    }
    if (targetDll.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("No target DLL found for IDS texts.");
        return false;
    }
    int newId = currentId;
    if (!IdsDataService::writeStringEntry(dataset, targetDll, currentId, text, &newId, errorMessage))
        return false;
    if (outId)
        *outId = newId;
    m_idsTextByNumber.insert(QString::number(newId), text);
    return true;
}

bool NewsRumorEditor::writeNewsFile(QString *errorMessage)
{
    IniDocument outDoc = m_newsDoc;
    for (const NewsEntry &entry : m_entries) {
        if (entry.removed)
            continue;
        int sectionIndex = entry.sectionIndex;
        if (sectionIndex < 0 || sectionIndex >= outDoc.size()) {
            IniSection section;
            section.name = QStringLiteral("NewsItem");
            section.entries.append({QStringLiteral("rank"), entry.rank});
            section.entries.append({QStringLiteral("icon"), entry.icon});
            section.entries.append({QStringLiteral("logo"), entry.logo});
            section.entries.append({QStringLiteral("category"), QString::number(entry.categoryIds)});
            section.entries.append({QStringLiteral("headline"), QString::number(entry.headlineIds)});
            section.entries.append({QStringLiteral("text"), QString::number(entry.textIds)});
            for (const QString &base : entry.bases)
                section.entries.append({QStringLiteral("base"), base});
            if (entry.autoselect)
                section.entries.append({QStringLiteral("autoselect"), QString()});
            outDoc.append(section);
            continue;
        }
        IniSection &section = outDoc[sectionIndex];
        if (entry.modified) {
            setFirstValue(&section, QStringLiteral("rank"), entry.rank);
            setFirstValue(&section, QStringLiteral("icon"), entry.icon);
            setFirstValue(&section, QStringLiteral("logo"), entry.logo);
            setFirstValue(&section, QStringLiteral("category"), QString::number(entry.categoryIds));
            setFirstValue(&section, QStringLiteral("headline"), QString::number(entry.headlineIds));
            setFirstValue(&section, QStringLiteral("text"), QString::number(entry.textIds));
            replaceRepeatedValues(&section, QStringLiteral("base"), entry.bases);
            setFlagEntry(&section, QStringLiteral("autoselect"), entry.autoselect);
        }
    }

    for (const NewsEntry &entry : m_entries) {
        if (!entry.removed || entry.sectionIndex < 0 || entry.sectionIndex >= outDoc.size())
            continue;
        outDoc[entry.sectionIndex].name = QStringLiteral("__deleted__");
        outDoc[entry.sectionIndex].entries.clear();
    }
    for (int i = outDoc.size() - 1; i >= 0; --i) {
        if (outDoc.at(i).name == QStringLiteral("__deleted__"))
            outDoc.removeAt(i);
    }

    QFile file(m_newsPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = tr("news.ini could not be written: %1").arg(m_newsPath);
        return false;
    }
    file.write(IniParser::serialize(outDoc).toUtf8());
    file.close();
    m_newsDoc = outDoc;
    return true;
}

void NewsRumorEditor::addNews()
{
    setOperationProgress(0, tr("Creating news item..."));
    applyDetailToCurrentEntry();
    setOperationProgress(35, tr("Preparing default news fields..."));
    NewsEntry entry;
    entry.type = NewsEntry::News;
    entry.source = m_newsPath;
    entry.sectionIndex = -1;
    entry.rank = QStringLiteral("base_0_rank,mission_end");
    entry.icon = QStringLiteral("world");
    entry.logo = QStringLiteral("news_manhattan");
    entry.headlineText = tr("New News");
    entry.bodyText = tr("Enter news text");
    entry.headlineTextDirty = true;
    entry.bodyTextDirty = true;
    entry.bases = detailBases();
    if (entry.bases.isEmpty()) {
        const int row = m_baseTable->currentRow();
        if (row > 0 && m_baseTable->item(row, BaseNameColumn)) {
            const int idx = m_baseTable->item(row, BaseNameColumn)->data(Qt::UserRole).toInt();
            if (idx > 0 && idx < m_bases.size())
                entry.bases.append(m_bases.at(idx).nickname);
        }
    }
    entry.modified = true;
    m_entries.append(entry);
    updateTechnicalFieldCompletions();
    setOperationProgress(70, tr("Refreshing news list..."));
    rebuildBaseCounts();
    populateBaseTable();
    populateNewsTable();
    m_newsTable->setCurrentCell(m_entries.size() - 1, 0);
    setDirty(true);
    setOperationProgress(100, tr("News item created."));
}

void NewsRumorEditor::removeSelectedNews()
{
    applyDetailToCurrentEntry();
    const int idx = selectedEntryIndex();
    if (idx < 0 || idx >= m_entries.size())
        return;
    DeleteNewsDialog dialog(m_entries.at(idx), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    setOperationProgress(0, tr("Deleting news item..."));
    m_entries[idx].removed = true;
    m_entries[idx].modified = true;
    m_currentEntryIndex = -1;
    setOperationProgress(60, tr("Refreshing news list..."));
    rebuildBaseCounts();
    populateBaseTable();
    populateNewsTable();
    showEntryInDetail(-1);
    setDirty(true);
    setOperationProgress(100, tr("News item deleted."));
}

void NewsRumorEditor::parseIniContent(const QString &content, const QString &source)
{
    const IniDocument doc = IniParser::parseText(content);
    for (int i = 0; i < doc.size(); ++i) {
        const IniSection &section = doc.at(i);
        if (section.name.compare(QStringLiteral("NewsItem"), Qt::CaseInsensitive) == 0) {
            appendEntryFromSection(section, i, source);
            NewsEntry &entry = m_entries.last();
            const QString inlineHeadline = section.value(QStringLiteral("news")).trimmed();
            const QString inlineText = section.value(QStringLiteral("text")).trimmed();
            bool ok = false;
            inlineText.toInt(&ok);
            if (!inlineHeadline.isEmpty() && entry.headlineText.isEmpty())
                entry.headlineText = inlineHeadline;
            if (!inlineText.isEmpty() && !ok && entry.bodyText.isEmpty()) {
                entry.bodyText = inlineText;
                entry.text = inlineText;
            }
            entry.originalHeadlineText = entry.headlineText;
            entry.originalBodyText = entry.bodyText;
            continue;
        }

        if (section.name.compare(QStringLiteral("MBNpc"), Qt::CaseInsensitive) != 0)
            continue;
        for (const IniEntry &iniEntry : section.entries) {
            if (iniEntry.first.compare(QStringLiteral("rumor"), Qt::CaseInsensitive) != 0 &&
                iniEntry.first.compare(QStringLiteral("rumor_string"), Qt::CaseInsensitive) != 0) {
                continue;
            }
            NewsEntry entry;
            entry.type = NewsEntry::Rumor;
            entry.source = source;
            entry.sectionIndex = i;
            bool ok = false;
            const int ids = iniEntry.second.trimmed().toInt(&ok);
            if (ok) {
                entry.ids = ids;
                entry.textIds = ids;
                entry.bodyText = resolvedIdsText(ids);
            } else {
                entry.text = iniEntry.second.trimmed();
                entry.bodyText = entry.text;
            }
            entry.originalBodyText = entry.bodyText;
            entry.searchBlob = QStringLiteral("%1 %2").arg(entry.text, entry.bodyText).toLower();
            m_entries.append(entry);
        }
    }
}

void NewsRumorEditor::loadFromFile(const QString &filePath)
{
    clearData();
    m_newsPath = filePath;
    m_newsDoc = IniParser::parseFile(filePath);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusLabel->setText(tr("Cannot open file: %1").arg(filePath));
        return;
    }
    QTextStream stream(&file);
    parseIniContent(stream.readAll(), filePath);
    populateBaseTable();
    populateNewsTable();
    emit titleChanged(tr("News Editor: %1").arg(QFileInfo(filePath).fileName()));
    refreshStatus();
}

void NewsRumorEditor::setEntries(const QVector<NewsEntry> &entries)
{
    m_entries = entries;
    for (NewsEntry &entry : m_entries) {
        if (entry.bodyText.isEmpty())
            entry.bodyText = entry.text;
        if (entry.headlineText.isEmpty() && entry.type == NewsEntry::News)
            entry.headlineText = entry.text;
        entry.originalHeadlineText = entry.headlineText;
        entry.originalBodyText = entry.bodyText;
        entry.searchBlob = QStringLiteral("%1 %2 %3 %4")
            .arg(entry.source, entry.text, entry.headlineText, entry.bodyText)
            .toLower();
    }
    updateTechnicalFieldCompletions();
    populateBaseTable();
    populateNewsTable();
    refreshStatus();
}

bool NewsRumorEditor::isModified() const
{
    if (m_dirty)
        return true;
    for (const NewsEntry &entry : m_entries) {
        if (entry.modified)
            return true;
    }
    return false;
}

QString NewsRumorEditor::resolvedIdsText(int ids) const
{
    if (ids <= 0)
        return {};
    return m_idsTextByNumber.value(QString::number(ids)).trimmed();
}

QString NewsRumorEditor::baseDisplay(const QString &nickname) const
{
    const int idx = m_baseIndexByKey.value(keyOf(nickname), -1);
    if (idx >= 0 && idx < m_bases.size()) {
        const QString display = m_bases.at(idx).displayName.trimmed();
        if (!display.isEmpty() && display.compare(nickname, Qt::CaseInsensitive) != 0)
            return QStringLiteral("%1 - %2").arg(nickname, display);
    }
    return nickname;
}

QString NewsRumorEditor::basesDisplay(const NewsEntry &entry) const
{
    if (entry.bases.isEmpty())
        return tr("Global");
    QStringList labels;
    labels.reserve(entry.bases.size());
    for (const QString &base : entry.bases)
        labels.append(baseDisplay(base));
    return clippedTableText(labels.join(QStringLiteral("; ")), 140);
}

QString NewsRumorEditor::newsPreview(const NewsEntry &entry) const
{
    QString text = entry.bodyText.simplified();
    if (text.isEmpty())
        text = tr("<IDS %1 missing>").arg(entry.textIds);
    return clippedTableText(text, 140);
}

QString NewsRumorEditor::clippedTableText(const QString &text, int maxChars) const
{
    const QString clean = text.simplified();
    if (clean.size() <= maxChars)
        return clean;
    return clean.left(qMax(0, maxChars - 3)).trimmed() + QStringLiteral("...");
}

QStringList NewsRumorEditor::detailBases() const
{
    QString raw = m_basesEdit->toPlainText();
    raw.replace(QLatin1Char(','), QLatin1Char('\n'));
    QStringList result;
    QSet<QString> seen;
    for (const QString &line : raw.split(QLatin1Char('\n'))) {
        const QString base = line.trimmed();
        if (base.isEmpty())
            continue;
        const QString key = keyOf(base);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        result.append(base);
    }
    return result;
}

QStringList NewsRumorEditor::invalidBases(const QStringList &bases) const
{
    QStringList invalid;
    QSet<QString> seen;
    for (const QString &base : bases) {
        const QString clean = base.trimmed();
        if (clean.isEmpty())
            continue;
        const QString key = keyOf(clean);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        if (!m_baseIndexByKey.contains(key))
            invalid.append(clean);
    }
    return invalid;
}

int NewsRumorEditor::selectedEntryIndex() const
{
    const int row = m_newsTable ? m_newsTable->currentRow() : -1;
    if (row < 0 || row >= m_newsTable->rowCount())
        return -1;
    auto *item = m_newsTable->item(row, NewsHeadlineColumn);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void NewsRumorEditor::setDirty(bool dirty)
{
    m_dirty = dirty;
    if (m_saveButton)
        m_saveButton->setEnabled(!m_newsPath.isEmpty());
    emit titleChanged(dirty ? tr("News Editor*") : tr("News Editor"));
}

} // namespace flatlas::editors
