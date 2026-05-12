#include "FieldTemplateGenerator.h"

#include "infrastructure/parser/IniParser.h"

#include <QFileInfo>
#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>

namespace flatlas::tools {

namespace {

using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

QString number(double value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

const IniSection *findSection(const IniDocument &doc, const QString &name)
{
    for (const IniSection &section : doc) {
        if (section.name.compare(name, Qt::CaseInsensitive) == 0)
            return &section;
    }
    return nullptr;
}

QVector<const IniSection *> findSections(const IniDocument &doc, const QString &name)
{
    QVector<const IniSection *> sections;
    for (const IniSection &section : doc) {
        if (section.name.compare(name, Qt::CaseInsensitive) == 0)
            sections.append(&section);
    }
    return sections;
}

int intValue(const IniSection *section, const QString &key, int fallback)
{
    if (!section)
        return fallback;
    bool ok = false;
    const int value = section->value(key).trimmed().toInt(&ok);
    return ok ? value : fallback;
}

double doubleValue(const IniSection *section, const QString &key, double fallback)
{
    if (!section)
        return fallback;
    bool ok = false;
    const double value = section->value(key).trimmed().toDouble(&ok);
    return ok ? value : fallback;
}

QColor colorValue(const IniSection *section, const QString &key, const QColor &fallback)
{
    if (!section)
        return fallback;
    const QStringList parts = section->value(key).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return fallback;
    bool okR = false;
    bool okG = false;
    bool okB = false;
    const int r = parts.at(0).trimmed().toInt(&okR);
    const int g = parts.at(1).trimmed().toInt(&okG);
    const int b = parts.at(2).trimmed().toInt(&okB);
    if (!okR || !okG || !okB)
        return fallback;
    return QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

FieldTemplateKind inferAsteroidKind(const IniDocument &doc, const FieldTemplate &field)
{
    QStringList flags;
    for (const IniSection *section : findSections(doc, QStringLiteral("properties")))
        flags.append(section->values(QStringLiteral("flag")));
    const QString joined = flags.join(QLatin1Char(' ')).toLower();
    const QString details = QStringList{field.fileName, field.texturePanelsFile, field.billboardShape,
                                        field.cubeShapeFallbacks.join(QLatin1Char(' '))}
                                .join(QLatin1Char(' '))
                                .toLower();
    if (joined.contains(QStringLiteral("mine")) || details.contains(QStringLiteral("mine")))
        return FieldTemplateKind::Mine;
    if (joined.contains(QStringLiteral("debris")) || details.contains(QStringLiteral("debris")))
        return FieldTemplateKind::Debris;
    if (joined.contains(QStringLiteral("ice")) || details.contains(QStringLiteral("ice")))
        return FieldTemplateKind::Ice;
    if (joined.contains(QStringLiteral("gas")) || details.contains(QStringLiteral("gas"))
        || details.contains(QStringLiteral("oxygen")))
        return FieldTemplateKind::Gas;
    return FieldTemplateKind::Asteroid;
}

QVector<FieldPlacedObject> parseCubeObjects(const IniDocument &doc)
{
    QVector<FieldPlacedObject> objects;
    for (const IniSection *section : findSections(doc, QStringLiteral("Cube"))) {
        for (const auto &entry : section->entries) {
            if (entry.first.compare(QStringLiteral("asteroid"), Qt::CaseInsensitive) != 0)
                continue;
            const QStringList parts = entry.second.split(QLatin1Char(','), Qt::SkipEmptyParts);
            if (parts.size() < 7)
                continue;
            bool okX = false;
            bool okY = false;
            bool okZ = false;
            bool okRx = false;
            bool okRy = false;
            bool okRz = false;
            FieldPlacedObject object;
            object.assetNickname = parts.at(0).trimmed();
            object.x = parts.at(1).trimmed().toDouble(&okX);
            object.y = parts.at(2).trimmed().toDouble(&okY);
            object.z = parts.at(3).trimmed().toDouble(&okZ);
            object.rotateX = parts.at(4).trimmed().toInt(&okRx);
            object.rotateY = parts.at(5).trimmed().toInt(&okRy);
            object.rotateZ = parts.at(6).trimmed().toInt(&okRz);
            object.mineRole = parts.size() > 7 && parts.at(7).trimmed().compare(QStringLiteral("mine"), Qt::CaseInsensitive) == 0;
            if (okX && okY && okZ && okRx && okRy && okRz && !object.assetNickname.isEmpty())
                objects.append(object);
        }
    }
    return objects;
}

QStringList asteroidFlags(FieldTemplateKind kind)
{
    switch (kind) {
    case FieldTemplateKind::Debris:
        return {QStringLiteral("debris_objects"), QStringLiteral("Object_density_med")};
    case FieldTemplateKind::Mine:
        return {QStringLiteral("MINE_DANGER_OBJECTS"), QStringLiteral("Object_density_high"),
                QStringLiteral("DANGER_DENSITY_HIGH")};
    case FieldTemplateKind::Gas:
        return {QStringLiteral("rock_objects"), QStringLiteral("Object_density_med"),
                QStringLiteral("danger_density_med"), QStringLiteral("gas_danger_objects")};
    case FieldTemplateKind::Ice:
        return {QStringLiteral("ice_objects"), QStringLiteral("Object_density_med")};
    case FieldTemplateKind::Asteroid:
    case FieldTemplateKind::Nebula:
        return {QStringLiteral("rock_objects"), QStringLiteral("Object_density_med")};
    }
    return {};
}

QVector<FieldPlacedObject> fallbackObjects(const FieldTemplate &field)
{
    static const double positions[][3] = {
        {0.6, 0.2, -0.2},
        {0.2, 0.8, 0.3},
        {-0.3, -0.3, 0.8},
        {-0.2, -0.1, -0.6},
        {0.2, 0.4, -0.2},
        {-0.7, 0.4, -0.4},
    };
    static const int rotations[][3] = {
        {35, 10, 20},
        {45, 20, 0},
        {85, 0, 185},
        {105, 160, 25},
        {105, 160, 25},
        {75, 30, 70},
    };

    QVector<FieldPlacedObject> objects;
    const QStringList shapes = field.cubeShapeFallbacks.isEmpty()
        ? QStringList{QStringLiteral("minedout_asteroid10"), QStringLiteral("minedout_asteroid30")}
        : field.cubeShapeFallbacks;
    for (int i = 0; i < 6; ++i) {
        FieldPlacedObject object;
        object.assetNickname = shapes.at(i % shapes.size());
        object.x = positions[i][0];
        object.y = positions[i][1];
        object.z = positions[i][2];
        object.rotateX = rotations[i][0];
        object.rotateY = rotations[i][1];
        object.rotateZ = rotations[i][2];
        object.mineRole = field.kind == FieldTemplateKind::Mine || field.kind == FieldTemplateKind::Gas;
        objects.append(object);
    }
    return objects;
}

} // namespace

QString FieldTemplateGenerator::kindKey(FieldTemplateKind kind)
{
    switch (kind) {
    case FieldTemplateKind::Asteroid: return QStringLiteral("asteroid");
    case FieldTemplateKind::Ice: return QStringLiteral("ice");
    case FieldTemplateKind::Debris: return QStringLiteral("debris");
    case FieldTemplateKind::Mine: return QStringLiteral("mine");
    case FieldTemplateKind::Gas: return QStringLiteral("gas");
    case FieldTemplateKind::Nebula: return QStringLiteral("nebula");
    }
    return QStringLiteral("asteroid");
}

QString FieldTemplateGenerator::kindLabel(FieldTemplateKind kind)
{
    switch (kind) {
    case FieldTemplateKind::Asteroid: return QStringLiteral("Asteroid Field");
    case FieldTemplateKind::Ice: return QStringLiteral("Ice Field");
    case FieldTemplateKind::Debris: return QStringLiteral("Debris Field");
    case FieldTemplateKind::Mine: return QStringLiteral("Minefield");
    case FieldTemplateKind::Gas: return QStringLiteral("Gas Pocket");
    case FieldTemplateKind::Nebula: return QStringLiteral("Nebula");
    }
    return QStringLiteral("Asteroid Field");
}

QString FieldTemplateGenerator::linkedSectionName(FieldTemplateKind kind)
{
    return kind == FieldTemplateKind::Nebula ? QStringLiteral("Nebula") : QStringLiteral("Asteroids");
}

QString FieldTemplateGenerator::relativeDirectory(FieldTemplateKind kind)
{
    return kind == FieldTemplateKind::Nebula
        ? QStringLiteral("solar\\NEBULA")
        : QStringLiteral("solar\\ASTEROIDS");
}

QString FieldTemplateGenerator::defaultFileName(FieldTemplateKind kind)
{
    return QStringLiteral("custom_%1_field.ini").arg(kindKey(kind));
}

QString FieldTemplateGenerator::defaultZoneNickname(FieldTemplateKind kind)
{
    return QStringLiteral("Zone_custom_%1_field").arg(kindKey(kind));
}

QVector<FieldTemplate> FieldTemplateGenerator::presets()
{
    QVector<FieldTemplate> result;

    FieldTemplate rock;
    rock.kind = FieldTemplateKind::Asteroid;
    rock.presetName = QStringLiteral("Rock");
    rock.fileName = QStringLiteral("custom_rock_field.ini");
    rock.zoneNickname = QStringLiteral("Zone_custom_rock_field");
    rock.texturePanelsFile = QStringLiteral("solar\\asteroids\\rock_shapes.ini");
    rock.billboardShape = QStringLiteral("mine_rock_tri");
    rock.cubeShapeFallbacks = {QStringLiteral("minedout_asteroid10"), QStringLiteral("minedout_asteroid30"),
                               QStringLiteral("minedout_asteroid60")};
    rock.spacedust = QStringLiteral("asteroiddust");
    rock.music = QStringLiteral("zone_field_asteroid_rock");
    result.append(rock);

    FieldTemplate ice = rock;
    ice.kind = FieldTemplateKind::Ice;
    ice.presetName = QStringLiteral("Ice");
    ice.fileName = QStringLiteral("custom_ice_field.ini");
    ice.zoneNickname = QStringLiteral("Zone_custom_ice_field");
    ice.texturePanelsFile = QStringLiteral("solar\\asteroids\\ice_shapes.ini");
    ice.billboardShape = QStringLiteral("asteroid_ice_4");
    ice.cubeShapeFallbacks = {QStringLiteral("ice_ast_chunk1"), QStringLiteral("ice_ast_chunk2"),
                              QStringLiteral("ice_ast_chunk3")};
    ice.spacedust = QStringLiteral("icedust");
    ice.music = QStringLiteral("zone_field_asteroid_ice");
    ice.propertyFlags = 65;
    ice.primaryColor = QColor(170, 218, 255);
    ice.ambientColor = QColor(118, 138, 150);
    result.append(ice);

    FieldTemplate debris = rock;
    debris.kind = FieldTemplateKind::Debris;
    debris.presetName = QStringLiteral("Debris");
    debris.fileName = QStringLiteral("custom_debris_field.ini");
    debris.zoneNickname = QStringLiteral("Zone_custom_debris_field");
    debris.texturePanelsFile = QStringLiteral("solar\\asteroids\\debris_shapes.ini");
    debris.billboardShape = QStringLiteral("debris_tri");
    debris.cubeShapeFallbacks = {QStringLiteral("debris_med1"), QStringLiteral("debris_large1"),
                                 QStringLiteral("debris_large3"), QStringLiteral("debris_small2")};
    debris.spacedust = QStringLiteral("debrisdust");
    debris.music = QStringLiteral("zone_field_debris");
    debris.propertyFlags = 130;
    debris.visit = 36;
    debris.emptyCubeFrequency = 0.8;
    debris.dynamicCount = 20;
    debris.primaryColor = QColor(182, 175, 160);
    debris.ambientColor = QColor(112, 109, 104);
    result.append(debris);

    FieldTemplate mine = rock;
    mine.kind = FieldTemplateKind::Mine;
    mine.presetName = QStringLiteral("Minefield");
    mine.fileName = QStringLiteral("custom_minefield.ini");
    mine.zoneNickname = QStringLiteral("Zone_custom_minefield");
    mine.texturePanelsFile = QStringLiteral("solar\\asteroids\\mine_shapes.ini");
    mine.billboardShape = QStringLiteral("spike_mine_tri");
    mine.cubeShapeFallbacks = {QStringLiteral("mine_spike1")};
    mine.spacedust = QStringLiteral("debrisdust");
    mine.music = QStringLiteral("zone_field_mine");
    mine.propertyFlags = 4128;
    mine.damage = 250;
    mine.cubeSize = 275;
    mine.fillDistance = 2000;
    mine.emptyCubeFrequency = 0.25;
    mine.dynamicCount = 0;
    mine.primaryColor = QColor(214, 216, 255);
    mine.ambientColor = QColor(96, 101, 128);
    result.append(mine);

    FieldTemplate gas = rock;
    gas.kind = FieldTemplateKind::Gas;
    gas.presetName = QStringLiteral("Gas Pocket");
    gas.fileName = QStringLiteral("custom_gas_pocket.ini");
    gas.zoneNickname = QStringLiteral("Zone_custom_gas_pocket");
    gas.cubeShapeFallbacks = {QStringLiteral("mine_oxygen2")};
    gas.spacedust = QStringLiteral("radioactivedust_red");
    gas.music = QStringLiteral("zone_badlands");
    gas.propertyFlags = 16466;
    gas.visit = 0;
    gas.damage = 40;
    gas.fillDistance = 2400;
    gas.emptyCubeFrequency = 0.5;
    gas.primaryColor = QColor(171, 99, 56);
    gas.ambientColor = QColor(120, 53, 28);
    result.append(gas);

    FieldTemplate nebula;
    nebula.kind = FieldTemplateKind::Nebula;
    nebula.presetName = QStringLiteral("Generic Nebula");
    nebula.fileName = QStringLiteral("custom_nebula.ini");
    nebula.zoneNickname = QStringLiteral("Zone_custom_nebula");
    nebula.texturePanelsFile = QStringLiteral("solar\\nebula\\generic_shapes.ini");
    nebula.billboardShape = QStringLiteral("generic_cloud1");
    nebula.fillShape = QStringLiteral("nebula_circle2");
    nebula.cubeShapeFallbacks = {QStringLiteral("generic_cloud1"), QStringLiteral("generic_cloud2"),
                                 QStringLiteral("generic_cloud3"), QStringLiteral("generic_cloud4")};
    nebula.spacedust = QStringLiteral("radioactivedust_blue");
    nebula.music = QStringLiteral("zone_nebula_crow");
    nebula.propertyFlags = 32768;
    nebula.primaryColor = QColor(53, 105, 157);
    nebula.ambientColor = QColor(41, 77, 104);
    nebula.fogColor = QColor(24, 56, 92);
    result.append(nebula);

    return result;
}

FieldTemplate FieldTemplateGenerator::preset(FieldTemplateKind kind)
{
    const QVector<FieldTemplate> all = presets();
    for (const FieldTemplate &field : all) {
        if (field.kind == kind)
            return field;
    }
    return all.isEmpty() ? FieldTemplate{} : all.first();
}

FieldTemplate FieldTemplateGenerator::parseFieldIni(const QString &fileName, const QString &iniText, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    const IniDocument doc = IniParser::parseText(iniText);
    const bool isNebula = findSection(doc, QStringLiteral("Fog")) || findSection(doc, QStringLiteral("Clouds"))
        || findSection(doc, QStringLiteral("Exterior"));
    FieldTemplate field = preset(isNebula ? FieldTemplateKind::Nebula : FieldTemplateKind::Asteroid);
    field.fileName = normalizedFileName(fileName, field.kind);
    field.zoneNickname = defaultZoneNickname(field.kind);
    field.placedObjects.clear();

    const IniSection *texturePanels = findSection(doc, QStringLiteral("TexturePanels"));
    if (texturePanels)
        field.texturePanelsFile = texturePanels->value(QStringLiteral("file"), field.texturePanelsFile).trimmed();

    if (isNebula) {
        const IniSection *fog = findSection(doc, QStringLiteral("Fog"));
        const IniSection *exterior = findSection(doc, QStringLiteral("Exterior"));
        const IniSection *light = findSection(doc, QStringLiteral("NebulaLight"));
        const IniSection *clouds = findSection(doc, QStringLiteral("Clouds"));
        const IniSection *lightning = findSection(doc, QStringLiteral("BackgroundLightning"));
        field.fogDistance = intValue(fog, QStringLiteral("distance"), field.fogDistance);
        field.fogColor = colorValue(fog, QStringLiteral("color"), field.fogColor);
        field.fillShape = exterior ? exterior->value(QStringLiteral("fill_shape"), field.fillShape).trimmed() : field.fillShape;
        field.billboardShape = field.fillShape;
        field.primaryColor = colorValue(exterior, QStringLiteral("color"), field.primaryColor);
        field.ambientColor = colorValue(light, QStringLiteral("ambient"), field.ambientColor);
        field.puffCount = intValue(clouds, QStringLiteral("puff_count"), field.puffCount);
        field.lightningDuration = doubleValue(lightning, QStringLiteral("duration"), field.lightningDuration);
        field.lightningGap = doubleValue(lightning, QStringLiteral("gap"), field.lightningGap);
        if (clouds) {
            const QStringList shapes = clouds->values(QStringLiteral("puff_shape"));
            if (!shapes.isEmpty()) {
                field.cubeShapeFallbacks.clear();
                for (const QString &shape : shapes) {
                    const QString trimmed = shape.trimmed();
                    if (!trimmed.isEmpty())
                        field.cubeShapeFallbacks.append(trimmed);
                }
            }
        }
        return field;
    }

    const IniSection *fieldSection = findSection(doc, QStringLiteral("Field"));
    const IniSection *billboards = findSection(doc, QStringLiteral("AsteroidBillboards"));
    const IniSection *dynamic = findSection(doc, QStringLiteral("DynamicAsteroids"));
    field.cubeSize = intValue(fieldSection, QStringLiteral("cube_size"), field.cubeSize);
    field.fillDistance = intValue(fieldSection, QStringLiteral("fill_dist"), field.fillDistance);
    field.emptyCubeFrequency = doubleValue(fieldSection, QStringLiteral("empty_cube_frequency"), field.emptyCubeFrequency);
    field.primaryColor = colorValue(fieldSection, QStringLiteral("diffuse_color"), field.primaryColor);
    field.ambientColor = colorValue(fieldSection, QStringLiteral("ambient_color"), field.ambientColor);
    field.billboardCount = intValue(billboards, QStringLiteral("count"), field.billboardCount);
    field.billboardShape = billboards ? billboards->value(QStringLiteral("shape"), field.billboardShape).trimmed() : field.billboardShape;
    field.dynamicCount = intValue(dynamic, QStringLiteral("count"), field.dynamicCount);
    field.placedObjects = parseCubeObjects(doc);
    if (!field.placedObjects.isEmpty()) {
        field.cubeShapeFallbacks.clear();
        for (const FieldPlacedObject &object : std::as_const(field.placedObjects)) {
            if (!field.cubeShapeFallbacks.contains(object.assetNickname, Qt::CaseInsensitive))
                field.cubeShapeFallbacks.append(object.assetNickname);
        }
    }
    field.kind = inferAsteroidKind(doc, field);
    field.zoneNickname = defaultZoneNickname(field.kind);
    return field;
}

QString FieldTemplateGenerator::generateFieldIni(const FieldTemplate &field)
{
    QStringList lines;
    auto appendBlank = [&lines]() {
        if (!lines.isEmpty() && !lines.last().isEmpty())
            lines.append(QString());
    };

    lines << QStringLiteral("[TexturePanels]")
          << QStringLiteral("file = %1").arg(field.texturePanelsFile);
    appendBlank();

    if (field.kind == FieldTemplateKind::Nebula) {
        const QStringList cloudShapes = field.cubeShapeFallbacks.isEmpty()
            ? QStringList{QStringLiteral("generic_cloud1"), QStringLiteral("generic_cloud2"),
                          QStringLiteral("generic_cloud3"), QStringLiteral("generic_cloud4")}
            : field.cubeShapeFallbacks;
        auto cloudAt = [&cloudShapes](int index) {
            return cloudShapes.at(std::min(index, static_cast<int>(cloudShapes.size()) - 1));
        };
        auto exteriorFor = [](QString shape) {
            return shape.replace(QStringLiteral("cloud"), QStringLiteral("exterior"), Qt::CaseInsensitive);
        };

        lines << QStringLiteral("[Fog]")
              << QStringLiteral("fog_enabled = 1")
              << QStringLiteral("near = 0")
              << QStringLiteral("distance = %1").arg(field.fogDistance)
              << QStringLiteral("color = %1").arg(colorText(field.fogColor));
        appendBlank();
        lines << QStringLiteral("[properties]")
              << QStringLiteral("flag = nebula");
        appendBlank();
        lines << QStringLiteral("[Exterior]")
              << QStringLiteral("shape = %1").arg(exteriorFor(cloudAt(0)))
              << QStringLiteral("shape = %1").arg(exteriorFor(cloudAt(1)))
              << QStringLiteral("shape = %1").arg(exteriorFor(cloudAt(2)))
              << QStringLiteral("shape = %1").arg(exteriorFor(cloudAt(3)))
              << QStringLiteral("shape_weights = 1, 1, 1, 1")
              << QStringLiteral("fill_shape = %1").arg(field.fillShape.isEmpty() ? QStringLiteral("nebula_circle2") : field.fillShape)
              << QStringLiteral("plane_slices = 3")
              << QStringLiteral("bit_radius = 10000")
              << QStringLiteral("bit_radius_random_variation = 0.2")
              << QStringLiteral("min_bits = 5")
              << QStringLiteral("max_bits = 8")
              << QStringLiteral("move_bit_percent = 0.75")
              << QStringLiteral("equator_bias = 0.5")
              << QStringLiteral("color = %1").arg(colorText(field.primaryColor));
        appendBlank();
        lines << QStringLiteral("[NebulaLight]")
              << QStringLiteral("ambient = %1").arg(colorText(field.ambientColor))
              << QStringLiteral("sun_burnthrough_intensity = 0.5")
              << QStringLiteral("sun_burnthrough_scaler = 2");
        appendBlank();
        lines << QStringLiteral("[Clouds]")
              << QStringLiteral("max_distance = 300")
              << QStringLiteral("puff_count = %1").arg(field.puffCount)
              << QStringLiteral("puff_radius = 100")
              << QStringLiteral("puff_colora = %1").arg(colorText(field.primaryColor))
              << QStringLiteral("puff_colorb = 255, 255, 255")
              << QStringLiteral("puff_max_alpha = 0.5")
              << QStringLiteral("puff_shape = %1").arg(cloudAt(0))
              << QStringLiteral("puff_shape = %1").arg(cloudAt(1))
              << QStringLiteral("puff_shape = %1").arg(cloudAt(2))
              << QStringLiteral("puff_shape = %1").arg(cloudAt(3))
              << QStringLiteral("puff_weights = 1, 1, 1, 1")
              << QStringLiteral("puff_drift = 1")
              << QStringLiteral("near_fade_distance = 125, 200");
        appendBlank();
        lines << QStringLiteral("[BackgroundLightning]")
              << QStringLiteral("duration = %1").arg(number(field.lightningDuration))
              << QStringLiteral("gap = %1").arg(number(field.lightningGap))
              << QStringLiteral("color = %1").arg(colorText(field.ambientColor));
        return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    }

    lines << QStringLiteral("[Field]")
          << QStringLiteral("cube_size = %1").arg(field.cubeSize)
          << QStringLiteral("fill_dist = %1").arg(field.fillDistance)
          << QStringLiteral("diffuse_color = %1").arg(colorText(field.primaryColor))
          << QStringLiteral("ambient_color = %1").arg(colorText(field.ambientColor))
          << QStringLiteral("ambient_increase = 70, 40, 30")
          << QStringLiteral("empty_cube_frequency = %1").arg(number(field.emptyCubeFrequency));
    appendBlank();

    lines << QStringLiteral("[properties]");
    for (const QString &flag : asteroidFlags(field.kind))
        lines << QStringLiteral("flag = %1").arg(flag);
    appendBlank();

    lines << QStringLiteral("[Cube]");
    const QVector<FieldPlacedObject> objects = field.placedObjects.isEmpty() ? fallbackObjects(field) : field.placedObjects;
    for (const FieldPlacedObject &object : objects) {
        QString value = QStringLiteral("asteroid = %1, %2, %3, %4, %5, %6, %7")
                            .arg(object.assetNickname,
                                 number(object.x),
                                 number(object.y),
                                 number(object.z))
                            .arg(object.rotateX)
                            .arg(object.rotateY)
                            .arg(object.rotateZ);
        if (object.mineRole || field.kind == FieldTemplateKind::Mine || field.kind == FieldTemplateKind::Gas)
            value += QStringLiteral(", mine");
        lines << value;
    }
    appendBlank();

    lines << QStringLiteral("[Band]")
          << QStringLiteral("render_parts = 6")
          << QStringLiteral("shape = asteroid_belt_04")
          << QStringLiteral("height = 4000")
          << QStringLiteral("offset_dist = 2000")
          << QStringLiteral("fade = 1, 1.35, 15, 17")
          << QStringLiteral("texture_aspect = 1")
          << QStringLiteral("color_shift = 0.8, 0.8, 0.8")
          << QStringLiteral("ambient_intensity = 1")
          << QStringLiteral("vert_increase = 2");
    appendBlank();

    lines << QStringLiteral("[AsteroidBillboards]")
          << QStringLiteral("count = %1").arg(field.billboardCount)
          << QStringLiteral("start_dist = 2000")
          << QStringLiteral("fade_dist_percent = 0.35")
          << QStringLiteral("shape = %1").arg(field.billboardShape)
          << QStringLiteral("color_shift = 0.8, 0.8, 0.8")
          << QStringLiteral("ambient_intensity = 1")
          << QStringLiteral("size = 70, 90");

    if (field.dynamicCount > 0) {
        appendBlank();
        lines << QStringLiteral("[DynamicAsteroids]")
              << QStringLiteral("asteroid = DAsteroid_mineable_small1")
              << QStringLiteral("count = %1").arg(field.dynamicCount)
              << QStringLiteral("placement_radius = 150")
              << QStringLiteral("placement_offset = 90")
              << QStringLiteral("max_velocity = 15")
              << QStringLiteral("max_angular_velocity = 3")
              << QStringLiteral("color_shift = 1, 1, 1");
    }

    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString FieldTemplateGenerator::generateSystemLinkPreview(const FieldTemplate &field)
{
    QStringList lines;
    lines << QStringLiteral("[%1]").arg(linkedSectionName(field.kind))
          << QStringLiteral("file = %1\\%2").arg(relativeDirectory(field.kind), normalizedFileName(field.fileName, field.kind))
          << QStringLiteral("zone = %1").arg(field.zoneNickname)
          << QString()
          << QStringLiteral("[Zone]")
          << QStringLiteral("nickname = %1").arg(field.zoneNickname)
          << QStringLiteral("ids_name = 0")
          << QStringLiteral("pos = 0, 0, 0")
          << QStringLiteral("shape = ELLIPSOID")
          << QStringLiteral("size = 10000, 8000, 10000")
          << QStringLiteral("property_flags = %1").arg(field.propertyFlags)
          << QStringLiteral("ids_info = 66146")
          << QStringLiteral("visit = %1").arg(field.visit)
          << QStringLiteral("damage = %1").arg(field.damage);
    if (!field.spacedust.isEmpty())
        lines << QStringLiteral("spacedust = %1").arg(field.spacedust);
    if (!field.music.isEmpty())
        lines << QStringLiteral("Music = %1").arg(field.music);
    lines << QStringLiteral("sort = %1").arg(field.sort);
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString FieldTemplateGenerator::normalizedFileName(QString fileName, FieldTemplateKind kind)
{
    fileName = QFileInfo(fileName.trimmed()).fileName();
    if (fileName.isEmpty())
        fileName = defaultFileName(kind);
    if (!fileName.endsWith(QStringLiteral(".ini"), Qt::CaseInsensitive))
        fileName += QStringLiteral(".ini");
    return fileName;
}

QVector<FieldPlacedObject> FieldTemplateGenerator::autoDistribute(const QVector<FieldAsset> &assets,
                                                                  FieldTemplateKind kind,
                                                                  int count,
                                                                  quint32 seed,
                                                                  const QString &spread)
{
    QVector<FieldPlacedObject> result;
    if (assets.isEmpty() || count <= 0)
        return result;

    QRandomGenerator random(seed == 0 ? 1 : seed);
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        const FieldAsset &asset = assets.at(i % assets.size());
        double x = random.generateDouble() * 2.0 - 1.0;
        double y = random.generateDouble() * 2.0 - 1.0;
        double z = random.generateDouble() * 2.0 - 1.0;
        if (spread.compare(QStringLiteral("Belt"), Qt::CaseInsensitive) == 0) {
            y *= 0.18;
        } else if (spread.compare(QStringLiteral("Shell"), Qt::CaseInsensitive) == 0) {
            const double length = std::max(0.001, std::sqrt(x * x + y * y + z * z));
            x /= length;
            y /= length;
            z /= length;
        }

        FieldPlacedObject object;
        object.assetNickname = asset.nickname;
        object.x = x;
        object.y = y;
        object.z = z;
        object.rotateX = static_cast<int>(random.bounded(0, 181));
        object.rotateY = static_cast<int>(random.bounded(0, 181));
        object.rotateZ = static_cast<int>(random.bounded(0, 181));
        object.mineRole = kind == FieldTemplateKind::Mine
            || kind == FieldTemplateKind::Gas
            || asset.nickname.contains(QStringLiteral("mine"), Qt::CaseInsensitive);
        result.append(object);
    }
    return result;
}

QString FieldTemplateGenerator::colorText(const QColor &color)
{
    return QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue());
}

} // namespace flatlas::tools
