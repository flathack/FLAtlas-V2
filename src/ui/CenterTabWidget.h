#pragma once
#include <QTabWidget>
namespace flatlas::ui {
class CenterTabWidget : public QTabWidget {
    Q_OBJECT
public:
    explicit CenterTabWidget(QWidget *parent = nullptr);
};
} // namespace flatlas::ui
