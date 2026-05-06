#pragma once
// core/ThemeColors.h - user-configurable colors shared by UI and rendering

#include <QColor>
#include <QString>
#include <QVector>

namespace flatlas::core {

struct ThemeColorChoice {
    QString key;
    QString label;
    QColor defaultColor;
};

class ThemeColors {
public:
    static QVector<ThemeColorChoice> choices();
    static QColor color(const QString &key);
    static void setColor(const QString &key, const QColor &color);
    static void resetToDefaults();
};

} // namespace flatlas::core
