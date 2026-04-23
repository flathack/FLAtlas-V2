#include "SolarObjectItem.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/io/CmpLoader.h"
#include "infrastructure/parser/IniParser.h"

#include <QBrush>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QPixmap>
#include <QPolygonF>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

namespace flatlas::rendering {

namespace {

constexpr qreal kPositionScale = 0.01;
constexpr qreal kTopViewIconRadiusBoost = 1.85;
constexpr qreal kTopViewIconMinRadius = 5.5;
constexpr qreal kHoverOutlinePadding = 3.0;
constexpr qreal kSelectionGlowPadding = 6.0;
constexpr int kTopViewIconRenderSize = 72;
constexpr auto kTopViewIconCacheVersion = "v1";

struct CachedTopViewAppearance {
    QPixmap pixmap;
    qreal modelRadius = 0.0;
    bool resolved = false;
};

QString normalizedKey(const QString &value)
{
    QString normalized = QDir::fromNativeSeparators(value).trimmed();
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

bool archetypeContainsAny(const QString &archetype, std::initializer_list<const char *> parts)
{
    const QString lowered = archetype.trimmed().toLower();
    for (const char *part : parts) {
        if (lowered.contains(QString::fromLatin1(part)))
            return true;
    }
    return false;
}

QColor outlineForColor(const QColor &color)
{
    return color.darker(135);
}

QPointF mapTopViewPoint(const QVector3D &point,
                        qreal centerX,
                        qreal centerZ,
                        qreal scale,
                        int iconSize)
{
    const qreal px = (iconSize * 0.5) + ((point.x() - centerX) * scale);
    const qreal py = (iconSize * 0.5) + ((point.z() - centerZ) * scale);
    return QPointF(px, py);
}

QColor shadeColor(const QColor &color, qreal shade, int geometryIndex)
{
    const qreal factor = std::clamp(shade, 0.45, 1.25);
    const qreal drift = 1.0 + (0.025 * static_cast<qreal>(geometryIndex % 5));

    QColor out(color);
    out.setRed(std::clamp(static_cast<int>(out.red() * factor * drift), 0, 255));
    out.setGreen(std::clamp(static_cast<int>(out.green() * factor), 0, 255));
    out.setBlue(std::clamp(static_cast<int>(out.blue() * (factor / drift)), 0, 255));
    return out;
}

void appendWorldVertices(const flatlas::infrastructure::ModelNode &node,
                         const QVector3D &parentTranslation,
                         const QQuaternion &parentRotation,
                         QVector<QVector3D> &positions,
                         QVector<QVector<int>> &triangles,
                         QVector<int> &triangleGeometryIndexes,
                         int &geometryIndex)
{
    const QQuaternion worldRotation = parentRotation * node.rotation;
    const QVector3D worldTranslation = parentTranslation + parentRotation.rotatedVector(node.origin);

    for (const auto &mesh : node.meshes) {
        if (mesh.vertices.size() < 3 || mesh.indices.size() < 3) {
            ++geometryIndex;
            continue;
        }

        const int vertexOffset = positions.size();
        positions.reserve(positions.size() + mesh.vertices.size());
        for (const auto &vertex : mesh.vertices)
            positions.append(worldTranslation + worldRotation.rotatedVector(vertex.position));

        for (int index = 0; index + 2 < mesh.indices.size(); index += 3) {
            const int a = static_cast<int>(mesh.indices[index]);
            const int b = static_cast<int>(mesh.indices[index + 1]);
            const int c = static_cast<int>(mesh.indices[index + 2]);
            if (a < 0 || b < 0 || c < 0
                || a >= mesh.vertices.size()
                || b >= mesh.vertices.size()
                || c >= mesh.vertices.size()) {
                continue;
            }

            triangles.append({vertexOffset + a, vertexOffset + b, vertexOffset + c});
            triangleGeometryIndexes.append(geometryIndex);
        }

        ++geometryIndex;
    }

    for (const auto &child : node.children)
        appendWorldVertices(child, worldTranslation, worldRotation, positions, triangles, triangleGeometryIndexes, geometryIndex);
}

QImage renderModelTopViewIcon(const flatlas::infrastructure::DecodedModel &model,
                              const QColor &fillColor,
                              const QColor &outlineColor,
                              qreal *modelRadius)
{
    QVector<QVector3D> positions;
    QVector<QVector<int>> triangles;
    QVector<int> triangleGeometryIndexes;
    int geometryIndex = 0;
    appendWorldVertices(model.rootNode,
                        QVector3D(0.0f, 0.0f, 0.0f),
                        QQuaternion(),
                        positions,
                        triangles,
                        triangleGeometryIndexes,
                        geometryIndex);

    if (positions.size() < 3 || triangles.isEmpty())
        return QImage();

    qreal minX = positions.first().x();
    qreal maxX = positions.first().x();
    qreal minY = positions.first().y();
    qreal maxY = positions.first().y();
    qreal minZ = positions.first().z();
    qreal maxZ = positions.first().z();
    for (const auto &point : positions) {
        minX = std::min(minX, static_cast<qreal>(point.x()));
        maxX = std::max(maxX, static_cast<qreal>(point.x()));
        minY = std::min(minY, static_cast<qreal>(point.y()));
        maxY = std::max(maxY, static_cast<qreal>(point.y()));
        minZ = std::min(minZ, static_cast<qreal>(point.z()));
        maxZ = std::max(maxZ, static_cast<qreal>(point.z()));
    }

    const qreal spanX = std::max<qreal>(maxX - minX, 1e-6);
    const qreal spanZ = std::max<qreal>(maxZ - minZ, 1e-6);
    const qreal spanY = std::max<qreal>(maxY - minY, 1e-6);
    const qreal centerX = (minX + maxX) * 0.5;
    const qreal centerZ = (minZ + maxZ) * 0.5;
    const qreal margin = kTopViewIconRenderSize * 0.14;
    const qreal drawSpan = std::max<qreal>(1.0, kTopViewIconRenderSize - (margin * 2.0));
    const qreal scale = drawSpan / std::max<qreal>(std::max(spanX, spanZ), 1.0);

    if (modelRadius)
        *modelRadius = std::max(spanX, spanZ) * 0.5 * kPositionScale;

    QImage image(kTopViewIconRenderSize, kTopViewIconRenderSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setPen(Qt::NoPen);

    QPainterPath mergedOutline;
    int drawnTriangles = 0;
    for (int index = 0; index < triangles.size(); ++index) {
        const auto &triangle = triangles[index];
        if (triangle.size() != 3)
            continue;

        const QVector3D &p0 = positions[triangle[0]];
        const QVector3D &p1 = positions[triangle[1]];
        const QVector3D &p2 = positions[triangle[2]];

        const QPolygonF polygon({
            mapTopViewPoint(p0, centerX, centerZ, scale, kTopViewIconRenderSize),
            mapTopViewPoint(p1, centerX, centerZ, scale, kTopViewIconRenderSize),
            mapTopViewPoint(p2, centerX, centerZ, scale, kTopViewIconRenderSize)
        });
        if (polygon.boundingRect().width() <= 0.0 && polygon.boundingRect().height() <= 0.0)
            continue;

        const qreal avgHeight = (p0.y() + p1.y() + p2.y()) / 3.0;
        const qreal shade = 0.70 + (0.30 * ((avgHeight - minY) / spanY));
        painter.setBrush(shadeColor(fillColor, shade, triangleGeometryIndexes.value(index)));
        painter.drawPolygon(polygon);

        QPainterPath path;
        path.addPolygon(polygon);
        mergedOutline = mergedOutline.united(path);
        ++drawnTriangles;
    }

    if (drawnTriangles <= 0)
        return QImage();

    QPen outlinePen(outlineColor);
    outlinePen.setWidthF(std::max<qreal>(1.0, static_cast<qreal>(kTopViewIconRenderSize) / 28.0));
    outlinePen.setJoinStyle(Qt::RoundJoin);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(outlinePen);
    painter.drawPath(mergedOutline.simplified());
    painter.end();

    return image;
}

QString topViewIconCacheRoot()
{
    static QString cacheRoot;
    if (!cacheRoot.isEmpty())
        return cacheRoot;

    cacheRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/system-top-view-icons");
    QDir().mkpath(cacheRoot);
    return cacheRoot;
}

QString topViewIconMetaPath(const QString &cachePath)
{
    return cachePath + QStringLiteral(".radius");
}

QString topViewIconCachePath(const QString &archetype, const QString &modelPath)
{
    const QFileInfo modelInfo(modelPath);
    const QString stamp = QStringLiteral("%1:%2")
        .arg(modelInfo.size())
        .arg(modelInfo.lastModified().toMSecsSinceEpoch());
    const QByteArray rawKey = QStringLiteral("%1||%2||%3||%4")
        .arg(QString::fromLatin1(kTopViewIconCacheVersion),
             normalizedKey(archetype),
             normalizedKey(modelPath),
             stamp)
        .toUtf8();
    const QByteArray digest = QCryptographicHash::hash(rawKey, QCryptographicHash::Sha1).toHex();
    return QDir(topViewIconCacheRoot()).filePath(QString::fromLatin1(digest) + QStringLiteral(".png"));
}

bool loadCachedTopViewAppearance(const QString &cachePath, CachedTopViewAppearance &appearance)
{
    if (cachePath.isEmpty() || !QFileInfo::exists(cachePath))
        return false;

    QPixmap pixmap(cachePath);
    if (pixmap.isNull())
        return false;

    appearance.pixmap = pixmap;

    QFile radiusFile(topViewIconMetaPath(cachePath));
    if (radiusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        bool ok = false;
        const qreal radius = QString::fromUtf8(radiusFile.readAll()).trimmed().toDouble(&ok);
        if (ok && radius > 0.0)
            appearance.modelRadius = radius;
    }

    return true;
}

void saveCachedTopViewAppearance(const QString &cachePath, const QImage &image, qreal modelRadius)
{
    if (cachePath.isEmpty() || image.isNull())
        return;

    QDir().mkpath(QFileInfo(cachePath).absolutePath());
    image.save(cachePath, "PNG");

    QFile radiusFile(topViewIconMetaPath(cachePath));
    if (radiusFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        radiusFile.write(QByteArray::number(modelRadius, 'f', 6));
}

const QHash<QString, QString> &solarArchetypeModelPaths()
{
    static QString cachedGamePath;
    static QHash<QString, QString> cache;

    const QString gamePath = normalizedKey(flatlas::core::EditingContext::instance().primaryGamePath());
    if (gamePath == cachedGamePath)
        return cache;

    cachedGamePath = gamePath;
    cache.clear();
    if (gamePath.isEmpty())
        return cache;

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
    const QString solarArchPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
    if (solarArchPath.isEmpty())
        return cache;

    const auto document = flatlas::infrastructure::IniParser::parseFile(solarArchPath);
    for (const auto &section : document) {
        const QString nickname = normalizedKey(section.value(QStringLiteral("nickname")));
        const QString modelRelativePath = section.value(QStringLiteral("DA_archetype")).trimmed();
        if (nickname.isEmpty() || modelRelativePath.isEmpty())
            continue;

        const QString modelPath = flatlas::core::PathUtils::ciResolvePath(dataDir, modelRelativePath);
        if (!modelPath.isEmpty())
            cache.insert(nickname, modelPath);
    }

    return cache;
}

CachedTopViewAppearance resolveTopViewAppearance(const flatlas::domain::SolarObject &obj,
                                                const QColor &baseColor)
{
    static QHash<QString, CachedTopViewAppearance> cache;

    const QString archetype = normalizedKey(obj.archetype());
    if (archetype.isEmpty())
        return {};

    if (obj.type() == flatlas::domain::SolarObject::Planet
        || archetypeContainsAny(archetype, {"planet", "moon"})) {
        return {};
    }

    const auto &modelPaths = solarArchetypeModelPaths();
    const QString modelPath = modelPaths.value(archetype);
    if (modelPath.isEmpty())
        return {};

    const QString cachePath = topViewIconCachePath(archetype, modelPath);
    const QString cacheKey = cachePath.isEmpty() ? archetype : cachePath;

    if (const auto it = cache.constFind(cacheKey); it != cache.constEnd())
        return it.value();

    CachedTopViewAppearance appearance;
    appearance.resolved = true;

    if (loadCachedTopViewAppearance(cachePath, appearance)) {
        cache.insert(cacheKey, appearance);
        return appearance;
    }

    const auto model = flatlas::infrastructure::CmpLoader::loadModel(modelPath);
    if (model.isValid()) {
        const QImage image = renderModelTopViewIcon(model,
                                                    baseColor.lighter(120),
                                                    outlineForColor(baseColor),
                                                    &appearance.modelRadius);
        if (!image.isNull()) {
            appearance.pixmap = QPixmap::fromImage(image);
            saveCachedTopViewAppearance(cachePath, image, appearance.modelRadius);
        }
    }

    cache.insert(cacheKey, appearance);
    return appearance;
}

} // namespace

SolarObjectItem::SolarObjectItem(const QString &nickname,
                                  flatlas::domain::SolarObject::Type objType,
                                  QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_nickname(nickname)
    , m_objType(objType)
{
    const qreal r = radiusForType(objType);
    m_baseRadius = r;
    m_currentRadius = r;
    setRect(-r, -r, 2 * r, 2 * r);

    const QColor col = colorForType(objType);
    setBrush(QBrush(col));
    setPen(QPen(col.darker(130), 0.5));

    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setToolTip(nickname);

    m_labelItem = new QGraphicsSimpleTextItem(nickname, this);
    m_labelItem->setBrush(QColor(220, 220, 230));
    m_labelItem->setFont(QFont(QStringLiteral("Segoe UI"), 7));
    m_labelItem->setPos(r + 3.0, -r - 2.0);
    m_labelItem->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

SolarObjectItem::~SolarObjectItem()
{
    delete m_hoverAnimation;
    m_hoverAnimation = nullptr;
}

QColor SolarObjectItem::colorForType(flatlas::domain::SolarObject::Type t)
{
    using T = flatlas::domain::SolarObject::Type;
    switch (t) {
    case T::Sun:         return QColor(0xFF, 0xD7, 0x00); // #FFD700
    case T::Planet:      return QColor(0x44, 0x88, 0xCC); // #4488CC
    case T::Station:     return QColor(0x44, 0xCC, 0x44); // #44CC44
    case T::JumpGate:    return QColor(0xFF, 0x44, 0x44); // #FF4444
    case T::JumpHole:    return QColor(0xFF, 0x88, 0x00); // #FF8800
    case T::TradeLane:   return QColor(0x00, 0xCC, 0xCC); // #00CCCC
    case T::DockingRing: return QColor(0xCC, 0x44, 0xCC); // #CC44CC
    case T::Wreck:       return QColor(0x88, 0x88, 0x88); // #888888
    default:             return QColor(0xCC, 0xCC, 0xCC); // #CCCCCC
    }
}

qreal SolarObjectItem::radiusForType(flatlas::domain::SolarObject::Type t)
{
    using T = flatlas::domain::SolarObject::Type;
    switch (t) {
    case T::Sun:       return 8.0;
    case T::Planet:    return 5.0;
    case T::Station:   return 4.0;
    case T::JumpGate:  return 3.5;
    case T::JumpHole:  return 3.5;
    case T::TradeLane: return 2.0;
    default:           return 3.0;
    }
}

qreal SolarObjectItem::radiusForObject(const flatlas::domain::SolarObject &obj, qreal modelRadius)
{
    static const QRegularExpression trailingNumber(QStringLiteral("_(\\d+(?:\\.\\d+)?)$"));

    const auto radiusFromArchetype = [&](const QString &archetype) -> qreal {
        const auto match = trailingNumber.match(archetype);
        if (!match.hasMatch())
            return 0.0;

        bool ok = false;
        const qreal value = match.captured(1).toDouble(&ok);
        if (!ok || value <= 0.0)
            return 0.0;

        return value * kPositionScale;
    };

    const QString archetype = obj.archetype().trimmed().toLower();
    if (obj.type() == flatlas::domain::SolarObject::Planet || archetype.contains(QStringLiteral("planet"))) {
        const qreal radius = radiusFromArchetype(archetype);
        if (radius > 0.0)
            return radius;
    }

    if (obj.type() == flatlas::domain::SolarObject::Sun || archetype.contains(QStringLiteral("sun")) || archetype.contains(QStringLiteral("star"))) {
        const qreal radius = radiusFromArchetype(archetype);
        if (radius > 0.0)
            return radius;
    }

    if (modelRadius > 0.0)
        return modelRadius;

    return radiusForType(obj.type());
}

qreal SolarObjectItem::displayRadiusForCurrentStyle() const
{
    qreal radius = std::max<qreal>(0.1, m_baseRadius);
    if (!m_topViewIcon.isNull() && m_objType != flatlas::domain::SolarObject::Planet && m_objType != flatlas::domain::SolarObject::Sun)
        radius = std::max<qreal>(kTopViewIconMinRadius, radius * kTopViewIconRadiusBoost);
    return radius;
}

void SolarObjectItem::applyRotationFromObject(const flatlas::domain::SolarObject &obj)
{
    const QVector3D rotation = obj.rotation();
    qreal yaw = rotation.y();

    constexpr qreal kTolerance = 0.25;
    if (std::abs(std::abs(rotation.x()) - 180.0) <= kTolerance
        && std::abs(std::abs(rotation.z()) - 180.0) <= kTolerance) {
        yaw += 180.0;
        if (yaw > 180.0)
            yaw -= 360.0;
        else if (yaw < -180.0)
            yaw += 360.0;
    }

    setRotation(-yaw);
    syncLabelRotation();
}

void SolarObjectItem::syncLabelRotation()
{
    // Labels must always be rendered horizontally and fully readable,
    // regardless of the parent's orientation. The label already has
    // QGraphicsItem::ItemIgnoresTransformations set, which causes it to
    // ignore inherited transformations (including the parent's rotation).
    // Forcing the label's own rotation to 0 guarantees a horizontal
    // baseline even if external code were to change it; the label's
    // offset position relative to the parent is still honoured, so it
    // continues to track the object cleanly.
    if (m_labelItem)
        m_labelItem->setRotation(0.0);
}

void SolarObjectItem::ensureHoverAnimation()
{
    if (m_hoverAnimation)
        return;

    auto *animation = new QVariantAnimation();
    animation->setDuration(180);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animation, &QVariantAnimation::valueChanged, animation,
                     [this](const QVariant &value) {
        m_hoverProgress = value.toReal();
        update();
    });
    m_hoverAnimation = animation;
}

QPen SolarObjectItem::hoverPen() const
{
    const qreal progress = std::clamp(m_hoverProgress, 0.0, 1.0);
    QColor color(120, 200, 255, static_cast<int>(70 + (150.0 * progress)));
    QPen pen(color, 1.5 + (1.5 * progress));
    pen.setCosmetic(true);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    return pen;
}

QPen SolarObjectItem::selectionGlowPen() const
{
    QPen pen(QColor(64, 170, 255, 230), 2.4);
    pen.setCosmetic(true);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    return pen;
}

void SolarObjectItem::updateFromObject(const flatlas::domain::SolarObject &obj)
{
    m_nickname = obj.nickname();
    m_archetype = obj.archetype();
    m_objType = obj.type();

    const QColor color = colorForType(obj.type());
    setBrush(QBrush(color));
    setPen(QPen(outlineForColor(color), 0.5));

    const CachedTopViewAppearance appearance = resolveTopViewAppearance(obj, color);
    m_topViewIcon = appearance.pixmap;
    m_baseRadius = radiusForObject(obj, appearance.modelRadius);
    m_currentRadius = displayRadiusForCurrentStyle();
    setRect(-m_currentRadius, -m_currentRadius, 2 * m_currentRadius, 2 * m_currentRadius);
    applyRotationFromObject(obj);
    setToolTip(m_archetype.isEmpty() ? m_nickname : QStringLiteral("%1\n%2").arg(m_nickname, m_archetype));
    if (m_labelItem) {
        m_labelItem->setText(m_nickname);
        m_labelItem->setPos(m_currentRadius + 3.0, -m_currentRadius - 2.0);
    }
    update();
}

void SolarObjectItem::setLabelVisibleForScale(qreal scale)
{
    // Labels should already be visible at the initial "fit to NavMap" zoom
    // level (~0.37 for a typical vanilla system), not only after the user
    // zooms deeply in. The threshold used to be 2.0, which effectively hid
    // every object label at the standard zoom.
    constexpr qreal kLabelScaleThreshold = 0.25;
    const bool shouldShow = m_objectVisibleByFilter
        && m_labelVisibleByFilter
        && (isSelected() || scale >= kLabelScaleThreshold);
    if (m_labelItem)
        m_labelItem->setVisible(shouldShow);
}

void SolarObjectItem::applyDisplayFilter(const SystemDisplayFilterSettings &settings, qreal scale)
{
    SolarObjectDisplayContext context;
    context.nickname = m_nickname;
    context.archetype = m_archetype;
    context.type = m_objType;

    bool objectVisible = settings.objectVisibleForType(m_objType);
    bool labelVisible = settings.labelVisibleForType(m_objType);

    for (const SystemDisplayFilterRule &rule : settings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;

        const bool show = rule.action == DisplayFilterAction::Show;
        if (rule.target == DisplayFilterTarget::Object || rule.target == DisplayFilterTarget::Both)
            objectVisible = show;
        if (rule.target == DisplayFilterTarget::Label || rule.target == DisplayFilterTarget::Both)
            labelVisible = show;
    }

    m_objectVisibleByFilter = objectVisible;
    m_labelVisibleByFilter = labelVisible;
    setVisible(m_objectVisibleByFilter);
    setLabelVisibleForScale(scale);
}

QRectF SolarObjectItem::boundingRect() const
{
    const QRectF rect = QGraphicsEllipseItem::boundingRect();
    const qreal extraPadding = isSelected() ? kSelectionGlowPadding : kHoverOutlinePadding;
    return rect.adjusted(-extraPadding, -extraPadding, extraPadding, extraPadding);
}

void SolarObjectItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget)
{
    const QRectF ellipseRect = rect();

    if (!m_topViewIcon.isNull()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(ellipseRect.toRect(), m_topViewIcon);
        painter->setPen(pen());
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(ellipseRect);
        if (m_hoverProgress > 0.001) {
            painter->setPen(hoverPen());
            painter->drawEllipse(ellipseRect.adjusted(-m_hoverProgress * 2.0,
                                                      -m_hoverProgress * 2.0,
                                                      m_hoverProgress * 2.0,
                                                      m_hoverProgress * 2.0));
        }
        if (isSelected()) {
            painter->setPen(selectionGlowPen());
            painter->drawEllipse(ellipseRect.adjusted(-2.5, -2.5, 2.5, 2.5));
        }
        painter->restore();
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QGraphicsEllipseItem::paint(painter, option, widget);
    if (m_hoverProgress > 0.001) {
        painter->setPen(hoverPen());
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(ellipseRect.adjusted(-m_hoverProgress * 2.0,
                                                  -m_hoverProgress * 2.0,
                                                  m_hoverProgress * 2.0,
                                                  m_hoverProgress * 2.0));
    }
    if (isSelected()) {
        painter->setPen(selectionGlowPen());
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(ellipseRect.adjusted(-2.5, -2.5, 2.5, 2.5));
    }
    painter->restore();
}

void SolarObjectItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    ensureHoverAnimation();
    prepareGeometryChange();
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

void SolarObjectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    ensureHoverAnimation();
    prepareGeometryChange();
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
    QGraphicsEllipseItem::hoverLeaveEvent(event);
}

} // namespace flatlas::rendering
