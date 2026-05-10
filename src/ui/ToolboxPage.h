#pragma once

#include <QWidget>

namespace flatlas::ui {

class ToolboxPage : public QWidget
{
    Q_OBJECT

public:
    explicit ToolboxPage(QWidget *parent = nullptr);

signals:
    void toolRequested(const QString &key);
};

} // namespace flatlas::ui
