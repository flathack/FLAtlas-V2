#pragma once

#include "FactionEditorService.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QTextEdit;
class QTreeWidget;

namespace flatlas::editors {

class FactionEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit FactionEditorPage(QWidget *parent = nullptr);
    bool save(QString *errorMessage = nullptr);
    bool isDirty() const;

signals:
    void titleChanged(const QString &title);
    void loadingProgressChanged(int percent, const QString &message);

private:
    void buildUi();
    void scheduleLoadFromContext();
    void loadFromContext();
    void reportLoadingProgress(int percent, const QString &message);
    void refreshIdsTextCache();
    void refreshFactionList();
    void selectFaction(const QString &nickname);
    void loadFactionToEditors(const QString &nickname);
    void saveEditorsToFaction();
    void refreshReputationTable();
    void refreshEmpathyTable();
    void refreshValidation();
    QString resolvedIdsText(const QString &idsValue) const;
    QString factionDisplayLabel(const QString &nickname) const;
    QString title() const;

    FactionEditorService *m_service = nullptr;
    QListWidget *m_factionList = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_idsNameEdit = nullptr;
    QLabel *m_idsNameResolvedLabel = nullptr;
    QLineEdit *m_idsInfoEdit = nullptr;
    QLabel *m_idsInfoResolvedLabel = nullptr;
    QLineEdit *m_idsShortNameEdit = nullptr;
    QLabel *m_idsShortNameResolvedLabel = nullptr;
    QCheckBox *m_initialWorldCheck = nullptr;
    QCheckBox *m_empathyCheck = nullptr;
    QCheckBox *m_factionPropCheck = nullptr;
    QLineEdit *m_legalityEdit = nullptr;
    QLineEdit *m_pluralityEdit = nullptr;
    QLineEdit *m_msgPrefixEdit = nullptr;
    QLineEdit *m_jumpPreferenceEdit = nullptr;
    QTextEdit *m_npcShipsEdit = nullptr;
    QTextEdit *m_voicesEdit = nullptr;
    QTextEdit *m_spaceCostumesEdit = nullptr;
    QTextEdit *m_formationsEdit = nullptr;
    QTableWidget *m_reputationTable = nullptr;
    QTableWidget *m_empathyTable = nullptr;
    QTreeWidget *m_validationTree = nullptr;
    QHash<int, QString> m_idsTextById;
    QString m_currentNickname;
    bool m_updating = false;
};

} // namespace flatlas::editors
