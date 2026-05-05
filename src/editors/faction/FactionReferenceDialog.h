#pragma once

#include "FactionEditorService.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace flatlas::editors {

class FactionReferenceDialog : public QDialog {
    Q_OBJECT
public:
    explicit FactionReferenceDialog(FactionEditorService *service,
                                    const QString &nickname,
                                    QWidget *parent = nullptr);

    QString replacementNickname() const;

signals:
    void factionChanged(const QString &nickname);

private:
    void refresh();
    bool runDeactivate();
    bool runDelete();

    FactionEditorService *m_service = nullptr;
    QString m_nickname;
    QLabel *m_summaryLabel = nullptr;
    QComboBox *m_replacementCombo = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_deactivateButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
};

} // namespace flatlas::editors
