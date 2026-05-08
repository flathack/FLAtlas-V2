#include "ActivityLogPage.h"

#include <QCalendarWidget>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace flatlas::ui {
namespace {

class LogCalendarWidget final : public QCalendarWidget
{
public:
    explicit LogCalendarWidget(QWidget *parent = nullptr)
        : QCalendarWidget(parent)
    {
        setGridVisible(true);
        setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        setMinimumWidth(230);
    }

    void setDayCounts(const QHash<QDate, int> &counts)
    {
        m_counts = counts;
        updateCells();
    }

protected:
    void paintCell(QPainter *painter, const QRect &rect, QDate date) const override
    {
        QCalendarWidget::paintCell(painter, rect, date);

        const int count = m_counts.value(date, 0);
        if (count <= 0)
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QString text = QString::number(count);
        const QFontMetrics metrics(painter->font());
        const int width = std::max(18, metrics.horizontalAdvance(text) + 8);
        const QRect badgeRect(rect.right() - width - 3, rect.bottom() - 18, width, 15);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(80, 145, 210, 220));
        painter->drawRoundedRect(badgeRect, 7, 7);
        painter->setPen(Qt::white);
        painter->drawText(badgeRect, Qt::AlignCenter, text);

        painter->restore();
    }

private:
    QHash<QDate, int> m_counts;
};

QString actionText(int count)
{
    return count == 1 ? QObject::tr("%1 action").arg(count)
                      : QObject::tr("%1 actions").arg(count);
}

} // namespace

ActivityLogPage::ActivityLogPage(const QString &logPath, QWidget *parent)
    : QWidget(parent)
    , m_logPath(QDir::cleanPath(logPath))
{
    setObjectName(QStringLiteral("activityLog"));
    setupUi();
    reload();
}

QString ActivityLogPage::logPath() const
{
    return m_logPath;
}

void ActivityLogPage::setLogPath(const QString &logPath)
{
    const QString cleanPath = QDir::cleanPath(logPath);
    if (m_logPath.compare(cleanPath, Qt::CaseInsensitive) == 0)
        return;

    m_logPath = cleanPath;
    if (m_pathLabel)
        m_pathLabel->setText(QFileInfo(m_logPath).absoluteFilePath());
    reload();
}

void ActivityLogPage::setupUi()
{
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    rootLayout->addWidget(splitter, 1);

    auto *logPanel = new QWidget(splitter);
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(10, 10, 8, 10);
    logLayout->setSpacing(8);

    auto *searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(logPanel);
    m_searchEdit->setPlaceholderText(tr("Search log..."));
    searchRow->addWidget(m_searchEdit, 1);
    m_reloadButton = new QPushButton(tr("Refresh"), logPanel);
    searchRow->addWidget(m_reloadButton);
    logLayout->addLayout(searchRow);

    m_logView = new QPlainTextEdit(logPanel);
    m_logView->setReadOnly(true);
    m_logView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    logLayout->addWidget(m_logView, 1);

    auto *sidebar = new QWidget(splitter);
    sidebar->setMinimumWidth(250);
    sidebar->setMaximumWidth(320);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(8, 10, 10, 10);
    sideLayout->setSpacing(8);

    auto *calendar = new LogCalendarWidget(sidebar);
    m_calendar = calendar;
    sideLayout->addWidget(m_calendar);

    auto *daysLabel = new QLabel(tr("Days"), sidebar);
    QFont daysFont = daysLabel->font();
    daysFont.setBold(true);
    daysLabel->setFont(daysFont);
    sideLayout->addWidget(daysLabel);

    m_dayList = new QListWidget(sidebar);
    m_dayList->setAlternatingRowColors(true);
    sideLayout->addWidget(m_dayList, 1);

    m_statusLabel = new QLabel(sidebar);
    m_statusLabel->setWordWrap(true);
    sideLayout->addWidget(m_statusLabel);

    m_pathLabel = new QLabel(QFileInfo(m_logPath).absoluteFilePath(), sidebar);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    sideLayout->addWidget(m_pathLabel);

    splitter->addWidget(logPanel);
    splitter->addWidget(sidebar);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({900, 280});

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ActivityLogPage::applyFilter);
    connect(m_reloadButton, &QPushButton::clicked, this, &ActivityLogPage::reload);
    connect(m_calendar, &QCalendarWidget::clicked, this, &ActivityLogPage::jumpToDate);
    connect(m_dayList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        jumpToDate(item->data(Qt::UserRole).toDate());
    });
}

void ActivityLogPage::reload()
{
    QFile file(m_logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_entries.clear();
        m_dayCounts.clear();
        applyFilter();
        rebuildDaySummary();
        if (m_statusLabel)
            m_statusLabel->setText(tr("Log file not found."));
        return;
    }

    parseLog(QString::fromUtf8(file.readAll()));
    rebuildDaySummary();
    applyFilter();
}

void ActivityLogPage::parseLog(const QString &content)
{
    m_entries.clear();
    m_dayCounts.clear();

    static const QRegularExpression linePattern(
        QStringLiteral("^\\[(\\d{4}-\\d{2}-\\d{2})\\s+[^\\]]+\\]\\s+\\[[^\\]]+\\]\\s+\\[[^\\]]+\\]\\s+.*$"));

    const QStringList lines = content.split(QLatin1Char('\n'));
    m_entries.reserve(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.isEmpty())
            continue;

        LogEntry entry;
        entry.line = line;
        entry.sourceLine = i;

        const QRegularExpressionMatch match = linePattern.match(line);
        if (match.hasMatch()) {
            entry.date = QDate::fromString(match.captured(1), Qt::ISODate);
            if (entry.date.isValid())
                ++m_dayCounts[entry.date];
        }

        m_entries.append(std::move(entry));
    }
}

void ActivityLogPage::rebuildDaySummary()
{
    static_cast<LogCalendarWidget *>(m_calendar)->setDayCounts(m_dayCounts);

    m_dayList->clear();
    QList<QDate> dates = m_dayCounts.keys();
    std::sort(dates.begin(), dates.end(), [](const QDate &left, const QDate &right) {
        return left > right;
    });

    for (const QDate &date : dates) {
        const int count = m_dayCounts.value(date);
        auto *item = new QListWidgetItem(QStringLiteral("%1  %2")
                                             .arg(date.toString(Qt::ISODate), actionText(count)));
        item->setData(Qt::UserRole, date);
        m_dayList->addItem(item);
    }
}

void ActivityLogPage::applyFilter()
{
    const QString needle = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    QStringList visibleLines;
    m_filteredLineBySourceLine.clear();

    for (const LogEntry &entry : std::as_const(m_entries)) {
        if (!needle.isEmpty() && !entry.line.contains(needle, Qt::CaseInsensitive))
            continue;
        m_filteredLineBySourceLine.insert(entry.sourceLine, visibleLines.size());
        visibleLines.append(entry.line);
    }

    m_logView->setPlainText(visibleLines.join(QLatin1Char('\n')));
    updateEmptyState();

    const QDate selectedDate = m_calendar ? m_calendar->selectedDate() : QDate();
    if (selectedDate.isValid() && m_dayCounts.contains(selectedDate))
        jumpToDate(selectedDate);
}

void ActivityLogPage::jumpToDate(const QDate &date)
{
    if (!date.isValid())
        return;

    if (m_calendar)
        m_calendar->setSelectedDate(date);

    for (const LogEntry &entry : std::as_const(m_entries)) {
        if (entry.date != date)
            continue;

        const auto it = m_filteredLineBySourceLine.constFind(entry.sourceLine);
        if (it == m_filteredLineBySourceLine.constEnd())
            continue;

        QTextBlock block = m_logView->document()->findBlockByNumber(*it);
        if (!block.isValid())
            break;

        QTextCursor cursor(block);
        m_logView->setTextCursor(cursor);
        m_logView->centerCursor();
        if (m_statusLabel)
            m_statusLabel->setText(tr("Selected day: %1").arg(date.toString(Qt::ISODate)));
        return;
    }

    if (m_statusLabel)
        m_statusLabel->setText(tr("No entries for selected day in current filter."));
}

void ActivityLogPage::updateEmptyState()
{
    if (!m_statusLabel)
        return;

    if (m_entries.isEmpty()) {
        m_statusLabel->setText(tr("No log entries."));
        return;
    }

    if (m_logView->document()->blockCount() == 1 && m_logView->toPlainText().isEmpty()) {
        m_statusLabel->setText(tr("No matching log entries."));
        return;
    }

    m_statusLabel->setText(tr("%1 actions").arg(m_logView->document()->blockCount()));
}

} // namespace flatlas::ui
