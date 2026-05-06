#pragma once

#include <QWidget>

namespace flatlas::rendering {

class ZoneLegendWidget : public QWidget {
    Q_OBJECT
public:
    explicit ZoneLegendWidget(QWidget *parent = nullptr);

    void refreshThemeColors();

private:
    void rebuild();
};

} // namespace flatlas::rendering
