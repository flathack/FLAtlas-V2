#pragma once
// rendering/view3d/ZoneColorScheme.h - coherent color decisions for 3D system zones

#include "domain/ZoneItem.h"

#include <QColor>
#include <QString>

namespace flatlas::rendering {

struct ZoneVisualStyle {
    QString category;
    QColor fillColor;
    QColor wireColor;
    QColor selectedFillColor;
    QColor selectedWireColor;
    float opacity = 0.18f;
    bool fillVisible = true;
};

class ZoneColorScheme {
public:
    static ZoneVisualStyle styleForZone(const flatlas::domain::ZoneItem &zone);
};

} // namespace flatlas::rendering
