#include "ToolIcons.h"

#include <QBrush>
#include <QFont>
#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

namespace {

QIcon drawIcon(const QColor &accent, const QString &glyph)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient background(0, 0, 64, 64);
    background.setColorAt(0.0, QColor(16, 26, 42, 245));
    background.setColorAt(1.0, QColor(5, 9, 18, 245));
    painter.setPen(QPen(accent.lighter(130), 2.0));
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(5, 5, 54, 54), 12, 12);

    painter.setPen(QPen(QColor(255, 255, 255, 28), 1.0));
    painter.drawLine(QPointF(15, 47), QPointF(49, 17));

    const QColor glow(accent.red(), accent.green(), accent.blue(), 70);
    painter.setPen(QPen(glow, 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawEllipse(QPointF(32, 32), 14, 14);

    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);

    if (glyph == QStringLiteral("universe")) {
        painter.drawEllipse(QPointF(32, 32), 9, 9);
        painter.setPen(QPen(accent.lighter(150), 2.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(32, 32), 19, 8);
        painter.drawEllipse(QPointF(32, 32), 8, 19);
    } else if (glyph == QStringLiteral("trade")) {
        painter.setPen(QPen(accent.lighter(150), 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF({QPointF(16, 40), QPointF(27, 28), QPointF(38, 36), QPointF(49, 22)}));
        painter.setBrush(accent);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(16, 40), 4, 4);
        painter.drawEllipse(QPointF(27, 28), 4, 4);
        painter.drawEllipse(QPointF(38, 36), 4, 4);
        painter.drawEllipse(QPointF(49, 22), 4, 4);
    } else if (glyph == QStringLiteral("ids")) {
        painter.setFont(QFont(QStringLiteral("Consolas"), 25, QFont::Bold));
        painter.setPen(accent.lighter(145));
        painter.drawText(QRectF(8, 11, 48, 42), Qt::AlignCenter, QStringLiteral("ID"));
    } else if (glyph == QStringLiteral("mod")) {
        painter.drawRoundedRect(QRectF(16, 18, 32, 28), 4, 4);
        painter.setPen(QPen(QColor(8, 12, 20), 3.0));
        painter.drawLine(QPointF(22, 25), QPointF(42, 25));
        painter.drawLine(QPointF(22, 34), QPointF(42, 34));
        painter.drawLine(QPointF(22, 43), QPointF(34, 43));
    } else if (glyph == QStringLiteral("settings")) {
        painter.setPen(QPen(accent.lighter(145), 4.0, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 8; ++i) {
            painter.save();
            painter.translate(32, 32);
            painter.rotate(i * 45.0);
            painter.drawLine(QPointF(0, -12), QPointF(0, -20));
            painter.restore();
        }
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(32, 32), 11, 11);
        painter.setBrush(QColor(8, 12, 20));
        painter.drawEllipse(QPointF(32, 32), 5, 5);
    } else if (glyph == QStringLiteral("npc")) {
        painter.drawEllipse(QPointF(32, 24), 9, 9);
        painter.drawRoundedRect(QRectF(19, 35, 26, 13), 6, 6);
        painter.setBrush(accent.lighter(145));
        painter.drawEllipse(QPointF(24, 29), 3, 3);
        painter.drawEllipse(QPointF(40, 29), 3, 3);
    } else if (glyph == QStringLiteral("faction")) {
        painter.setPen(QPen(accent.lighter(145), 3.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawEllipse(QPointF(24, 26), 7, 7);
        painter.drawEllipse(QPointF(42, 26), 7, 7);
        painter.drawEllipse(QPointF(33, 43), 7, 7);
        painter.drawLine(QPointF(29, 30), QPointF(37, 30));
        painter.drawLine(QPointF(27, 32), QPointF(31, 39));
        painter.drawLine(QPointF(39, 32), QPointF(35, 39));
    } else if (glyph == QStringLiteral("news")) {
        painter.drawRoundedRect(QRectF(18, 16, 28, 34), 3, 3);
        painter.setPen(QPen(QColor(8, 12, 20), 3.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(24, 25), QPointF(40, 25));
        painter.drawLine(QPointF(24, 33), QPointF(40, 33));
        painter.drawLine(QPointF(24, 41), QPointF(34, 41));
    } else if (glyph == QStringLiteral("activity")) {
        painter.setPen(QPen(accent.lighter(150), 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF({QPointF(15, 39), QPointF(24, 39), QPointF(29, 25), QPointF(36, 47), QPointF(42, 32), QPointF(50, 32)}));
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawEllipse(QPointF(24, 39), 3.5, 3.5);
        painter.drawEllipse(QPointF(29, 25), 3.5, 3.5);
        painter.drawEllipse(QPointF(36, 47), 3.5, 3.5);
        painter.drawEllipse(QPointF(42, 32), 3.5, 3.5);
    } else if (glyph == QStringLiteral("model3d")) {
        QPainterPath path;
        path.moveTo(32, 14);
        path.lineTo(48, 24);
        path.lineTo(48, 42);
        path.lineTo(32, 51);
        path.lineTo(16, 42);
        path.lineTo(16, 24);
        path.closeSubpath();
        painter.drawPath(path);
        painter.setPen(QPen(QColor(8, 12, 20), 3.0));
        painter.drawLine(QPointF(32, 14), QPointF(32, 32));
        painter.drawLine(QPointF(16, 24), QPointF(32, 32));
        painter.drawLine(QPointF(48, 24), QPointF(32, 32));
    } else if (glyph == QStringLiteral("system")) {
        painter.setPen(QPen(accent.lighter(145), 3.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(18, 42), QPointF(32, 22));
        painter.drawLine(QPointF(32, 22), QPointF(47, 39));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(18, 42), 5, 5);
        painter.drawEllipse(QPointF(32, 22), 7, 7);
        painter.drawEllipse(QPointF(47, 39), 5, 5);
    } else if (glyph == QStringLiteral("file")) {
        QPainterPath page;
        page.moveTo(19, 14);
        page.lineTo(39, 14);
        page.lineTo(47, 23);
        page.lineTo(47, 50);
        page.lineTo(19, 50);
        page.closeSubpath();
        painter.drawPath(page);
        painter.setPen(QPen(QColor(8, 12, 20), 2.5, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(25, 30), QPointF(41, 30));
        painter.drawLine(QPointF(25, 38), QPointF(39, 38));
    } else if (glyph == QStringLiteral("save")) {
        painter.drawRoundedRect(QRectF(18, 16, 28, 32), 3, 3);
        painter.setBrush(QColor(8, 12, 20));
        painter.drawRect(QRectF(24, 19, 15, 8));
        painter.drawRoundedRect(QRectF(23, 36, 18, 9), 2, 2);
    } else if (glyph == QStringLiteral("help")) {
        painter.setFont(QFont(QStringLiteral("Arial"), 31, QFont::Bold));
        painter.setPen(accent.lighter(145));
        painter.drawText(QRectF(8, 9, 48, 46), Qt::AlignCenter, QStringLiteral("?"));
    } else if (glyph == QStringLiteral("launch")) {
        QPainterPath ship;
        ship.moveTo(48, 32);
        ship.lineTo(18, 18);
        ship.lineTo(25, 32);
        ship.lineTo(18, 46);
        ship.closeSubpath();
        painter.drawPath(ship);
    } else if (glyph == QStringLiteral("close")) {
        painter.setPen(QPen(accent.lighter(150), 6.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(22, 22), QPointF(42, 42));
        painter.drawLine(QPointF(42, 22), QPointF(22, 42));
    } else {
        painter.drawEllipse(QPointF(32, 32), 14, 14);
    }

    painter.end();
    return QIcon(pixmap);
}

QIcon cachedIcon(const QString &key, const QColor &color, const QString &glyph)
{
    static QHash<QString, QIcon> cache;
    const QString cacheKey = QStringLiteral("%1:%2").arg(key, color.name(QColor::HexArgb));
    auto it = cache.constFind(cacheKey);
    if (it != cache.constEnd())
        return it.value();
    const QIcon icon = drawIcon(color, glyph);
    cache.insert(cacheKey, icon);
    return icon;
}

} // namespace

namespace flatlas::ui {

QIcon toolIcon(const QString &key)
{
    if (key == QStringLiteral("universe"))
        return cachedIcon(key, QColor(54, 168, 255), QStringLiteral("universe"));
    if (key == QStringLiteral("toolbox"))
        return cachedIcon(key, QColor(124, 188, 255), QStringLiteral("mod"));
    if (key == QStringLiteral("tradeRoutes"))
        return cachedIcon(key, QColor(89, 214, 145), QStringLiteral("trade"));
    if (key == QStringLiteral("idsEditor"))
        return cachedIcon(key, QColor(255, 205, 92), QStringLiteral("ids"));
    if (key == QStringLiteral("modManager"))
        return cachedIcon(key, QColor(230, 126, 34), QStringLiteral("mod"));
    if (key == QStringLiteral("modSettings"))
        return cachedIcon(key, QColor(171, 128, 255), QStringLiteral("settings"));
    if (key == QStringLiteral("npcEditor"))
        return cachedIcon(key, QColor(79, 220, 216), QStringLiteral("npc"));
    if (key == QStringLiteral("factionEditor"))
        return cachedIcon(key, QColor(247, 112, 142), QStringLiteral("faction"));
    if (key == QStringLiteral("newsRumorEditor"))
        return cachedIcon(key, QColor(255, 169, 79), QStringLiteral("news"));
    if (key == QStringLiteral("activity"))
        return cachedIcon(key, QColor(110, 210, 255), QStringLiteral("activity"));
    if (key == QStringLiteral("modelViewer") || key == QStringLiteral("system3d"))
        return cachedIcon(key, QColor(95, 186, 255), QStringLiteral("model3d"));
    if (key == QStringLiteral("fieldCreator"))
        return cachedIcon(key, QColor(141, 196, 116), QStringLiteral("system"));
    if (key == QStringLiteral("launch"))
        return launchIcon();
    if (key == QStringLiteral("systemEditor"))
        return cachedIcon(key, QColor(74, 196, 255), QStringLiteral("system"));
    if (key == QStringLiteral("iniEditor") || key == QStringLiteral("welcome"))
        return fileIcon();
    return cachedIcon(QStringLiteral("tool"), QColor(132, 178, 235), QStringLiteral("tool"));
}

QIcon fileIcon()
{
    return cachedIcon(QStringLiteral("file"), QColor(124, 188, 255), QStringLiteral("file"));
}

QIcon saveIcon()
{
    return cachedIcon(QStringLiteral("save"), QColor(89, 214, 145), QStringLiteral("save"));
}

QIcon settingsIcon()
{
    return cachedIcon(QStringLiteral("settings-command"), QColor(171, 128, 255), QStringLiteral("settings"));
}

QIcon helpIcon()
{
    return cachedIcon(QStringLiteral("help"), QColor(255, 205, 92), QStringLiteral("help"));
}

QIcon launchIcon()
{
    return cachedIcon(QStringLiteral("launch"), QColor(89, 214, 145), QStringLiteral("launch"));
}

QIcon closeIcon()
{
    return cachedIcon(QStringLiteral("close"), QColor(255, 118, 118), QStringLiteral("close"));
}

} // namespace flatlas::ui
