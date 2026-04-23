#pragma once

#include "ExclusionZoneUtils.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace flatlas::editors {

struct CreateExclusionZoneResult {
    QString nickname;
    QString fieldZoneNickname;
    QString fieldFileRelativePath;
    QString shape;
    QString comment;
    int sort = 99;
    bool linkToFieldZone = true;
    ExclusionShellSettings shellSettings;
};

class CreateExclusionZoneDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateExclusionZoneDialog(const QString &nicknameSuggestion,
                                       const QString &fieldZoneNickname,
                                       const QString &fieldFileRelativePath,
                                       bool supportsShell,
                                       const QStringList &shellOptions,
                                       QWidget *parent = nullptr);

    CreateExclusionZoneResult result() const { return m_result; }

protected:
    void accept() override;

private:
    void syncShellControls();
    static void configureSearchCompleter(QComboBox *combo);

    QString m_fieldZoneNickname;
    bool m_supportsShell = false;

    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_fieldZoneEdit = nullptr;
    QLineEdit *m_fieldFileEdit = nullptr;
    QComboBox *m_shapeCombo = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QSpinBox *m_sortSpin = nullptr;
    QCheckBox *m_linkCheck = nullptr;
    QCheckBox *m_shellEnabledCheck = nullptr;
    QSpinBox *m_shellFogFarSpin = nullptr;
    QComboBox *m_shellPathCombo = nullptr;
    QDoubleSpinBox *m_shellScalarSpin = nullptr;
    QDoubleSpinBox *m_shellMaxAlphaSpin = nullptr;
    QLineEdit *m_shellTintEdit = nullptr;

    CreateExclusionZoneResult m_result;
};

} // namespace flatlas::editors
