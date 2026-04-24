#pragma once

#include "SystemSettingsService.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace flatlas::editors {

class SystemSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SystemSettingsDialog(const SystemSettingsState &current,
                                  const SystemSettingsOptions &options,
                                  bool hasNonStandardSectionOrder,
                                  QWidget *parent = nullptr);

    SystemSettingsState result() const;
    bool shouldNormalizeSectionOrder() const;

public slots:
    void accept() override;

private:
    void pickColor(QLineEdit *edit);

    QString m_systemNickname;
    QLabel *m_systemLabel = nullptr;
    QComboBox *m_musicSpaceCombo = nullptr;
    QComboBox *m_musicDangerCombo = nullptr;
    QComboBox *m_musicBattleCombo = nullptr;
    QLineEdit *m_spaceColorEdit = nullptr;
    QPushButton *m_spaceColorButton = nullptr;
    QComboBox *m_localFactionCombo = nullptr;
    QLineEdit *m_ambientColorEdit = nullptr;
    QPushButton *m_ambientColorButton = nullptr;
    QComboBox *m_dustCombo = nullptr;
    QComboBox *m_backgroundBasicCombo = nullptr;
    QComboBox *m_backgroundComplexCombo = nullptr;
    QComboBox *m_backgroundNebulaeCombo = nullptr;
    QCheckBox *m_normalizeSectionsCheck = nullptr;
};

} // namespace flatlas::editors
