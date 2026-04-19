#pragma once
#include <QDialog>
namespace flatlas::ui {
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
};
} // namespace flatlas::ui
