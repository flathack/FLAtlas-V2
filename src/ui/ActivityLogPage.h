#pragma once

#include <QDate>
#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QCalendarWidget;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace flatlas::ui {

class ActivityLogPage : public QWidget
{
    Q_OBJECT

public:
    explicit ActivityLogPage(const QString &logPath, QWidget *parent = nullptr);

    QString logPath() const;
    void setLogPath(const QString &logPath);

public slots:
    void reload();

private slots:
    void applyFilter();
    void jumpToDate(const QDate &date);

private:
    struct LogEntry {
        QString line;
        QDate date;
        int sourceLine = 0;
    };

    void setupUi();
    void parseLog(const QString &content);
    void rebuildDaySummary();
    void updateEmptyState();

    QString m_logPath;
    QVector<LogEntry> m_entries;
    QHash<QDate, int> m_dayCounts;
    QHash<int, int> m_filteredLineBySourceLine;

    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_reloadButton = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QCalendarWidget *m_calendar = nullptr;
    QListWidget *m_dayList = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
};

} // namespace flatlas::ui
