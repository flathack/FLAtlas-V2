#pragma once

#include <QDialog>

#include "DeleteSystemService.h"

class QPushButton;
class QTextBrowser;

namespace flatlas::domain { class UniverseData; }

namespace flatlas::editors {

class DeleteSystemDialog : public QDialog {
    Q_OBJECT
public:
    DeleteSystemDialog(const QString &universeFilePath,
                       flatlas::domain::UniverseData *data,
                       const QString &systemNickname,
                       const QString &systemDisplayName,
                       QWidget *parent = nullptr);

    const DeleteSystemPrecheckReport &report() const { return m_report; }

private:
    void runPrecheck();
    void updateUiFromReport();
    QString formatReportHtml() const;

    QString m_universeFilePath;
    flatlas::domain::UniverseData *m_data = nullptr;
    QString m_systemNickname;
    QString m_systemDisplayName;
    DeleteSystemPrecheckReport m_report;
    QTextBrowser *m_reportView = nullptr;
    QPushButton *m_precheckButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
};

} // namespace flatlas::editors
