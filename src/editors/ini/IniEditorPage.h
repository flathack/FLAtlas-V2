#pragma once
// editors/ini/IniEditorPage.h – INI-Editor mit Syntax-Highlighting
// TODO Phase 6
#include <QWidget>
namespace flatlas::editors {
class IniEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit IniEditorPage(QWidget *parent = nullptr);
    void openFile(const QString &filePath);
};
} // namespace flatlas::editors
