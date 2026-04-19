#pragma once
#include <QWizard>
class QLineEdit;
class QComboBox;
class QSpinBox;

namespace flatlas::domain { class SystemDocument; }

namespace flatlas::editors {

class SystemCreationWizard : public QWizard {
    Q_OBJECT
public:
    explicit SystemCreationWizard(QWidget *parent = nullptr);

    /// Creates a SystemDocument from the wizard inputs. Caller owns the result.
    std::unique_ptr<flatlas::domain::SystemDocument> createDocument() const;

    QString systemNickname() const;
    QString systemDisplayName() const;

private:
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_displayNameEdit = nullptr;
    QComboBox *m_regionCombo = nullptr;
};

}
