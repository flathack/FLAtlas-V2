#pragma once
#include <QDialog>

class QComboBox;
class QLineEdit;
class QPushButton;

namespace flatlas::ui {
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onBrowsePath();

private:
    QComboBox  *m_themeCombo    = nullptr;
    QComboBox  *m_languageCombo = nullptr;
    QLineEdit  *m_gamePathEdit  = nullptr;
    QPushButton *m_browseButton = nullptr;
};
} // namespace flatlas::ui
