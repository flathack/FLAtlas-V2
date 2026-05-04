#pragma once

#include "ModExportService.h"

#include <QDialog>
#include <QSet>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace flatlas::editors {

class ModExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModExportDialog(const QString &profileName,
                             const QString &modRoot,
                             const QString &referenceRoot,
                             const QString &defaultDirectory,
                             QWidget *parent = nullptr);

    QString targetPath() const;
    int exportedCount() const { return m_exportedCount; }

private:
    void setupUi(const QString &profileName,
                 const QString &modRoot,
                 const QString &referenceRoot,
                 const QString &defaultDirectory);
    void chooseModRoot();
    void chooseReferenceRoot();
    void chooseTargetPath();
    void scan();
    void exportArchive();
    void showExclusions();
    void refreshFileTable();
    void refreshSummary();
    void refreshScriptXml();
    void updateTargetSuffix();
    ModExportPlan filteredPlan() const;

    QLineEdit *m_modRootEdit = nullptr;
    QLineEdit *m_referenceRootEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QLineEdit *m_targetEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_authorEdit = nullptr;
    QCheckBox *m_saveSafeCheck = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
    QPlainTextEdit *m_scriptEdit = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_fileTable = nullptr;
    QPushButton *m_exportButton = nullptr;

    ModExportPlan m_plan;
    bool m_hasPlan = false;
    bool m_scriptManuallyEdited = false;
    QSet<QString> m_manualExclusions;
    int m_exportedCount = 0;
};

} // namespace flatlas::editors
