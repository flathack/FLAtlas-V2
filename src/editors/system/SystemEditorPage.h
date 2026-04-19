#pragma once
// editors/system/SystemEditorPage.h – System-Editor
// TODO Phase 5
#include <QWidget>
namespace flatlas::editors {
class SystemEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit SystemEditorPage(QWidget *parent = nullptr);
};
} // namespace flatlas::editors
