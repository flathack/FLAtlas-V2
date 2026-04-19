#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
namespace flatlas::ui {
SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(500, 400);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Settings – Phase 3")));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
} // namespace flatlas::ui
