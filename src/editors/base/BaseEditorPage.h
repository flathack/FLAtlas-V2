#pragma once
#include <QWidget>
namespace flatlas::editors {
class BaseEditorPage : public QWidget { Q_OBJECT public: explicit BaseEditorPage(QWidget *parent = nullptr); };
} // namespace flatlas::editors
