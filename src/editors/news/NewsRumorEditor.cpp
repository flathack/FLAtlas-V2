#include "NewsRumorEditor.h"
#include "infrastructure/parser/IniParser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QRegularExpression>

namespace flatlas::editors {

// Column indices
enum Col { ColType = 0, ColSource, ColIds, ColText, ColCount };

NewsRumorEditor::NewsRumorEditor(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void NewsRumorEditor::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // --- Toolbar ---
    auto *toolbar = new QHBoxLayout;

    auto *loadBtn = new QPushButton(tr("Open File..."));
    toolbar->addWidget(loadBtn);

    m_addBtn = new QPushButton(tr("Add Entry"));
    toolbar->addWidget(m_addBtn);

    m_removeBtn = new QPushButton(tr("Remove"));
    toolbar->addWidget(m_removeBtn);

    toolbar->addStretch();

    toolbar->addWidget(new QLabel(tr("Filter:")));
    m_filterType = new QComboBox;
    m_filterType->addItem(tr("All"));
    m_filterType->addItem(tr("News"));
    m_filterType->addItem(tr("Rumors"));
    toolbar->addWidget(m_filterType);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search..."));
    m_searchEdit->setClearButtonEnabled(true);
    toolbar->addWidget(m_searchEdit);

    mainLayout->addLayout(toolbar);

    // --- Table ---
    m_table = new QTableWidget(0, ColCount);
    m_table->setHorizontalHeaderLabels({tr("Type"), tr("Source"), tr("IDS"), tr("Text")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(ColType, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColSource, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColIds, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table);

    // --- Status ---
    m_statusLabel = new QLabel(tr("No entries loaded"));
    mainLayout->addWidget(m_statusLabel);

    // --- Connections ---
    connect(loadBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Open News/Rumor File"), QString(),
            tr("INI Files (*.ini);;All Files (*)"));
        if (!path.isEmpty())
            loadFromFile(path);
    });

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &NewsRumorEditor::filterTable);

    connect(m_filterType, &QComboBox::currentIndexChanged,
            this, [this]() { filterTable(m_searchEdit->text()); });

    connect(m_table, &QTableWidget::cellChanged,
            this, &NewsRumorEditor::onCellChanged);

    connect(m_addBtn, &QPushButton::clicked,
            this, &NewsRumorEditor::addEntry);

    connect(m_removeBtn, &QPushButton::clicked,
            this, &NewsRumorEditor::removeSelectedEntry);
}

void NewsRumorEditor::loadFromFile(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusLabel->setText(tr("Cannot open: %1").arg(filePath));
        return;
    }
    QTextStream ts(&f);
    const QString content = ts.readAll();
    f.close();

    m_entries.clear();
    parseIniContent(content, filePath);
    populateTable();

    emit titleChanged(tr("News/Rumors: %1").arg(QFileInfo(filePath).fileName()));
    m_statusLabel->setText(tr("Loaded %1 entries from %2")
                               .arg(m_entries.size())
                               .arg(filePath));
}

void NewsRumorEditor::parseIniContent(const QString &content, const QString &source)
{
    // Simple INI-style parser: look for rumor/news keys
    // Freelancer format:
    //   [MBNpc] section with "rumor = ..." lines  → Rumor
    //   [NewsItem] section with "text = ..." lines → News
    static const QRegularExpression reSectionHeader(
        QStringLiteral("^\\s*\\[([^\\]]+)\\]"), QRegularExpression::MultilineOption);
    static const QRegularExpression reKeyValue(
        QStringLiteral("^\\s*(\\w+)\\s*=\\s*(.+)$"), QRegularExpression::MultilineOption);

    QString currentSection;
    const QStringList lines = content.split(QLatin1Char('\n'));

    for (const auto &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(';')))
            continue;

        auto secMatch = reSectionHeader.match(trimmed);
        if (secMatch.hasMatch()) {
            currentSection = secMatch.captured(1);
            continue;
        }

        auto kvMatch = reKeyValue.match(trimmed);
        if (!kvMatch.hasMatch())
            continue;

        const QString key = kvMatch.captured(1).toLower();
        const QString value = kvMatch.captured(2).trimmed();

        if (key == QLatin1String("rumor") || key == QLatin1String("rumor_string")) {
            NewsEntry entry;
            entry.type = NewsEntry::Rumor;
            entry.source = source;
            // Value might be an IDS number or inline text
            bool ok = false;
            int ids = value.toInt(&ok);
            if (ok) {
                entry.ids = ids;
            } else {
                entry.text = value;
            }
            m_entries.append(entry);
        } else if (key == QLatin1String("news") || key == QLatin1String("text")) {
            if (currentSection.toLower().contains(QLatin1String("news"))) {
                NewsEntry entry;
                entry.type = NewsEntry::News;
                entry.source = source;
                bool ok = false;
                int ids = value.toInt(&ok);
                if (ok) {
                    entry.ids = ids;
                } else {
                    entry.text = value;
                }
                m_entries.append(entry);
            }
        }
    }
}

void NewsRumorEditor::setEntries(const QVector<NewsEntry> &entries)
{
    m_entries = entries;
    populateTable();
    m_statusLabel->setText(tr("%1 entries").arg(m_entries.size()));
}

void NewsRumorEditor::populateTable()
{
    m_populating = true;
    m_table->setRowCount(m_entries.size());

    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &e = m_entries[i];

        auto *typeItem = new QTableWidgetItem(
            e.type == NewsEntry::News ? tr("News") : tr("Rumor"));
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, ColType, typeItem);

        auto *srcItem = new QTableWidgetItem(e.source);
        srcItem->setFlags(srcItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, ColSource, srcItem);

        auto *idsItem = new QTableWidgetItem(
            e.ids > 0 ? QString::number(e.ids) : QString());
        idsItem->setFlags(idsItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, ColIds, idsItem);

        m_table->setItem(i, ColText, new QTableWidgetItem(e.text));
    }

    m_populating = false;
    filterTable(m_searchEdit->text());
}

void NewsRumorEditor::filterTable(const QString &text)
{
    const int typeFilter = m_filterType->currentIndex(); // 0=All, 1=News, 2=Rumor
    const QString needle = text.toLower();

    for (int i = 0; i < m_table->rowCount(); ++i) {
        bool visible = true;

        // Type filter
        if (typeFilter == 1 && i < m_entries.size() && m_entries[i].type != NewsEntry::News)
            visible = false;
        if (typeFilter == 2 && i < m_entries.size() && m_entries[i].type != NewsEntry::Rumor)
            visible = false;

        // Text search
        if (visible && !needle.isEmpty()) {
            bool found = false;
            for (int c = 0; c < m_table->columnCount(); ++c) {
                auto *item = m_table->item(i, c);
                if (item && item->text().toLower().contains(needle)) {
                    found = true;
                    break;
                }
            }
            visible = found;
        }

        m_table->setRowHidden(i, !visible);
    }

    // Count visible
    int visibleCount = 0;
    for (int i = 0; i < m_table->rowCount(); ++i)
        if (!m_table->isRowHidden(i))
            ++visibleCount;

    m_statusLabel->setText(tr("Showing %1 of %2 entries")
                               .arg(visibleCount)
                               .arg(m_entries.size()));
}

void NewsRumorEditor::onCellChanged(int row, int col)
{
    if (m_populating || row < 0 || row >= m_entries.size())
        return;

    if (col == ColText) {
        auto *item = m_table->item(row, col);
        if (item) {
            m_entries[row].text = item->text();
            m_entries[row].modified = true;
        }
    }
}

bool NewsRumorEditor::isModified() const
{
    for (const auto &e : m_entries)
        if (e.modified)
            return true;
    return false;
}

void NewsRumorEditor::addEntry()
{
    NewsEntry entry;
    entry.type = NewsEntry::News;
    entry.source = tr("<new>");
    entry.modified = true;
    m_entries.append(entry);
    populateTable();
    m_table->scrollToBottom();
}

void NewsRumorEditor::removeSelectedEntry()
{
    const auto ranges = m_table->selectedRanges();
    if (ranges.isEmpty())
        return;

    // Collect rows in reverse order to avoid index shifts
    QVector<int> rows;
    for (const auto &range : ranges)
        for (int r = range.topRow(); r <= range.bottomRow(); ++r)
            rows.append(r);

    std::sort(rows.begin(), rows.end(), std::greater<int>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    for (int r : rows) {
        if (r >= 0 && r < m_entries.size())
            m_entries.removeAt(r);
    }

    populateTable();
}

} // namespace flatlas::editors
