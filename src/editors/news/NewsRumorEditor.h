#pragma once

#include "infrastructure/parser/IniParser.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTableWidget;
class QTableWidgetItem;

namespace flatlas::infrastructure { struct IdsDataset; }

namespace flatlas::editors {

struct NewsEntry {
    enum Type { News, Rumor };
    Type type = News;
    QString source;
    int ids = 0;
    QString text;
    bool modified = false;

    int sectionIndex = -1;
    int categoryIds = 0;
    int headlineIds = 0;
    int textIds = 0;
    QString categoryText;
    QString headlineText;
    QString bodyText;
    QString originalHeadlineText;
    QString originalBodyText;
    bool headlineTextDirty = false;
    bool bodyTextDirty = false;
    QStringList bases;
    QString rank;
    QString icon;
    QString logo;
    bool autoselect = false;
    bool removed = false;
    QString searchBlob;
};

struct NewsBaseRecord {
    QString nickname;
    QString displayName;
    QString system;
    int newsCount = 0;
    QString searchBlob;
};

class NewsRumorEditor : public QWidget {
    Q_OBJECT
public:
    explicit NewsRumorEditor(QWidget *parent = nullptr);

    void loadFromFile(const QString &filePath);
    void setEntries(const QVector<NewsEntry> &entries);
    QVector<NewsEntry> entries() const { return m_entries; }
    int entryCount() const { return m_entries.size(); }
    bool isModified() const;
    bool save();

signals:
    void titleChanged(const QString &title);

private:
    enum NewsColumn { NewsHeadlineColumn = 0, NewsPreviewColumn, NewsBasesColumn, NewsIconColumn, NewsRankColumn, NewsIssueColumn, NewsColumnCount };
    enum BaseColumn { BaseNameColumn = 0, BaseNicknameColumn, BaseSystemColumn, BaseNewsCountColumn, BaseColumnCount };

    void setupUi();
    void loadFromContext();
    bool loadWorkspace(const QString &gameRoot, QString *errorMessage = nullptr);
    void clearData();
    void loadIds(const QString &gameRoot);
    void loadBases(const QString &dataDir);
    void loadNewsFile(const QString &newsPath);
    void rebuildBaseCounts();
    void populateBaseTable();
    void populateNewsTable();
    void refreshFilters();
    void refreshStatus();
    void onBaseSelectionChanged();
    void onNewsSelectionChanged();
    void showEntryInDetail(int entryIndex);
    bool applyDetailToCurrentEntry();
    bool saveIdsText(int currentId, const QString &text, int *outId, QString *errorMessage);
    bool saveIdsText(const flatlas::infrastructure::IdsDataset &dataset,
                     const QString &targetDll,
                     int currentId,
                     const QString &text,
                     int *outId,
                     QString *errorMessage);
    bool writeNewsFile(QString *errorMessage);
    void addNews();
    void removeSelectedNews();
    void parseIniContent(const QString &content, const QString &source);
    void appendEntryFromSection(const flatlas::infrastructure::IniSection &section, int sectionIndex, const QString &source);
    QString resolvedIdsText(int ids) const;
    QString baseDisplay(const QString &nickname) const;
    QString basesDisplay(const NewsEntry &entry) const;
    QString newsPreview(const NewsEntry &entry) const;
    QString clippedTableText(const QString &text, int maxChars) const;
    QStringList detailBases() const;
    QStringList invalidBases(const QStringList &bases) const;
    int selectedEntryIndex() const;
    void setDirty(bool dirty);

    QLineEdit *m_baseSearchEdit = nullptr;
    QTableWidget *m_baseTable = nullptr;
    QLineEdit *m_newsSearchEdit = nullptr;
    QComboBox *m_scopeCombo = nullptr;
    QCheckBox *m_missingIdsOnly = nullptr;
    QTableWidget *m_newsTable = nullptr;
    QLineEdit *m_headlineEdit = nullptr;
    QPlainTextEdit *m_bodyEdit = nullptr;
    QPlainTextEdit *m_basesEdit = nullptr;
    QSpinBox *m_categorySpin = nullptr;
    QSpinBox *m_headlineSpin = nullptr;
    QSpinBox *m_textSpin = nullptr;
    QLineEdit *m_rankEdit = nullptr;
    QLineEdit *m_iconEdit = nullptr;
    QLineEdit *m_logoEdit = nullptr;
    QCheckBox *m_autoselectCheck = nullptr;
    QLabel *m_detailHintLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_saveButton = nullptr;

    flatlas::infrastructure::IniDocument m_newsDoc;
    QVector<NewsEntry> m_entries;
    QVector<NewsBaseRecord> m_bases;
    QHash<QString, int> m_baseIndexByKey;
    QHash<QString, QString> m_idsTextByNumber;
    QString m_gameRoot;
    QString m_newsPath;
    int m_currentEntryIndex = -1;
    bool m_dirty = false;
    bool m_populating = false;
    bool m_saving = false;
};

} // namespace flatlas::editors
