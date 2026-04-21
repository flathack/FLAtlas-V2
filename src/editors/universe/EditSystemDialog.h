#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace flatlas::domain { struct SystemInfo; }

namespace flatlas::editors {

struct EditSystemRequest {
    int visit = 0;
    QString name;
    QString infocardXml;
    double navMapScale = 1.36;
};

class EditSystemDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditSystemDialog(const flatlas::domain::SystemInfo &systemInfo,
                              const QString &currentResolvedName,
                              const QString &currentInfocardXml,
                              QWidget *parent = nullptr);

    EditSystemRequest request() const;

private:
    void updateInfocardPreview();

    QComboBox *m_visitCombo = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QDoubleSpinBox *m_navMapScaleSpin = nullptr;
    QPlainTextEdit *m_infocardEdit = nullptr;
    QLabel *m_infocardPreview = nullptr;
    QPushButton *m_wrapInfocardButton = nullptr;
};

} // namespace flatlas::editors
