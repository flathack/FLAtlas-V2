#pragma once
// editors/news/NewsRumorEditor.h – News & Rumor text editor
//
// Freelancer stores news/rumors in mbases.ini [MBNpc] blocks with "rumor" keys
// and news in news.ini. This editor loads them into a searchable table for
// editing.

#include <QWidget>
#include <QVector>

class QTableWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;

namespace flatlas::editors {

/// A single news or rumor entry.
struct NewsEntry {
    enum Type { News, Rumor };
    Type    type   = News;
    QString source;       // file path or base nickname
    int     ids    = 0;   // IDS reference (0 = inline text)
    QString text;         // the actual text content
    bool    modified = false;
};

class NewsRumorEditor : public QWidget {
    Q_OBJECT
public:
    explicit NewsRumorEditor(QWidget *parent = nullptr);

    /// Load entries from an INI file (news.ini or mbases.ini).
    void loadFromFile(const QString &filePath);

    /// Load entries programmatically.
    void setEntries(const QVector<NewsEntry> &entries);

    /// Return current entries.
    QVector<NewsEntry> entries() const { return m_entries; }

    /// Return number of entries.
    int entryCount() const { return m_entries.size(); }

    /// Return true if any entry was modified.
    bool isModified() const;

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void populateTable();
    void filterTable(const QString &text);
    void onCellChanged(int row, int col);
    void addEntry();
    void removeSelectedEntry();
    void parseIniContent(const QString &content, const QString &source);

    QLineEdit    *m_searchEdit  = nullptr;
    QComboBox    *m_filterType  = nullptr;
    QTableWidget *m_table       = nullptr;
    QLabel       *m_statusLabel = nullptr;
    QPushButton  *m_addBtn      = nullptr;
    QPushButton  *m_removeBtn   = nullptr;

    QVector<NewsEntry> m_entries;
    bool m_populating = false;  // guard against onCellChanged during populate
};

} // namespace flatlas::editors
