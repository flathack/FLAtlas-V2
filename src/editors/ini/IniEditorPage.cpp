#include "IniEditorPage.h"
#include <QVBoxLayout>
#include <QPlainTextEdit>
namespace flatlas::editors {
IniEditorPage::IniEditorPage(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    auto *editor = new QPlainTextEdit(this);
    editor->setPlaceholderText(tr("INI Editor – Phase 6"));
    layout->addWidget(editor);
}
void IniEditorPage::openFile(const QString &) {} // TODO Phase 6
} // namespace flatlas::editors
