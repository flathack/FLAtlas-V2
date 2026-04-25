// editors/system/SystemEditorPage.cpp – System-Editor (Phase 5)

#include "SystemEditorPage.h"
#include "SystemDisplayFilterDialog.h"
#include "SystemPersistence.h"
#include "SystemSettingsDialog.h"
#include "SystemSettingsService.h"
#include "BaseEditDialog.h"
#include "BaseEditService.h"
#include "PlanetCreationService.h"
#include "RingEditService.h"
#include "SystemUndoCommands.h"
#include "CreateObjectDialog.h"
#include "SystemCreationDialogs.h"
#include "CreateFieldZoneDialog.h"
#include "CreateExclusionZoneDialog.h"
#include "ExclusionZoneUtils.h"
#include "JumpConnectionService.h"
#include "TradeLaneEditService.h"
#include "editors/jump/JumpConnectionDialog.h"
#include "editors/universe/UniverseSerializer.h"
#include "core/Theme.h"
#include "editors/ini/IniCodeEditor.h"
#include "editors/ini/IniSyntaxHighlighter.h"

#include "core/Config.h"
#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"
#include "domain/ZoneItem.h"
#include "infrastructure/parser/IniParser.h"
#include "infrastructure/parser/XmlInfocard.h"
#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "rendering/preview/ModelCache.h"
#include "rendering/view2d/MapScene.h"
#include "rendering/view2d/SystemMapView.h"
#include "rendering/view2d/items/LightSourceItem.h"
#include "rendering/view2d/items/SolarObjectItem.h"
#include "rendering/view2d/items/ZoneItem2D.h"
#include "rendering/view3d/ModelViewport3D.h"
#include "rendering/view3d/SceneView3D.h"
#include "core/UndoManager.h"
#include "ui/MainWindow.h"

#include <QDialog>
#include <QApplication>
#include <QEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QAction>
#include <QActionGroup>
#include <QMessageBox>
#include <QInputDialog>
#include <QMetaEnum>
#include <QPixmap>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QHeaderView>
#include <QSet>
#include <QBrush>
#include <QFrame>
#include <QFileInfo>
#include <QDesktopServices>
#include <QFont>
#include <QPalette>
#include <QSaveFile>
#include <QDir>
#include <QDirIterator>
#include <QSignalBlocker>
#include <QUrl>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPen>
#include <QRegularExpression>
#include <QFile>
#include <QtMath>

#include <QQuaternion>

#include <algorithm>
#include <cmath>

using namespace flatlas::domain;
using namespace flatlas::rendering;
using namespace flatlas::infrastructure;

namespace flatlas::editors {

namespace {

using flatlas::infrastructure::IdsStringTable;

QStringList loadSolarArchetypesMatching(const QString &gameRoot, const QStringList &keywords)
{
    QStringList values;
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString solarArchPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
    if (solarArchPath.isEmpty())
        return values;

    const IniDocument doc = IniParser::parseFile(solarArchPath);
    QSet<QString> seen;
    for (const IniSection &section : doc) {
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (nickname.isEmpty())
            continue;
        const QString haystack = nickname.toLower();
        bool matches = keywords.isEmpty();
        for (const QString &keyword : keywords) {
            if (haystack.contains(keyword.toLower())) {
                matches = true;
                break;
            }
        }
        if (!matches || seen.contains(haystack))
            continue;
        seen.insert(haystack);
        values.append(nickname);
    }
    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList loadLoadoutsMatching(const QString &gameRoot, const QStringList &keywords)
{
    QStringList values;
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QStringList sources = {
        QStringLiteral("SHIPS/loadouts.ini"),
        QStringLiteral("SHIPS/loadouts_special.ini"),
        QStringLiteral("SOLAR/loadouts.ini"),
    };
    QSet<QString> seen;
    for (const QString &relativePath : sources) {
        const QString path = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
        if (path.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(path);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Loadout"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;
            const QString haystack = nickname.toLower();
            bool matches = keywords.isEmpty();
            for (const QString &keyword : keywords) {
                if (haystack.contains(keyword.toLower())) {
                    matches = true;
                    break;
                }
            }
            if (!matches || seen.contains(haystack))
                continue;
            seen.insert(haystack);
            values.append(nickname);
        }
    }
    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList loadLoadoutsWithPrefix(const QString &gameRoot, const QString &prefix)
{
    QStringList values;
    const QString normalizedPrefix = prefix.trimmed();
    if (normalizedPrefix.isEmpty())
        return values;

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QStringList sources = {
        QStringLiteral("SHIPS/loadouts.ini"),
        QStringLiteral("SHIPS/loadouts_special.ini"),
        QStringLiteral("SOLAR/loadouts.ini"),
    };
    QSet<QString> seen;
    for (const QString &relativePath : sources) {
        const QString path = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
        if (path.isEmpty())
            continue;

        const IniDocument doc = IniParser::parseFile(path);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Loadout"), Qt::CaseInsensitive) != 0)
                continue;

            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty()
                || !nickname.startsWith(normalizedPrefix, Qt::CaseInsensitive)) {
                continue;
            }

            const QString key = nickname.toLower();
            if (seen.contains(key))
                continue;
            seen.insert(key);
            values.append(nickname);
        }
    }

    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList loadFactionDisplays(const QString &gameRoot)
{
    QStringList values;
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString initialWorldPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("initialworld.ini"));
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    IdsStringTable ids;
    if (!exeDir.isEmpty())
        ids.loadFromFreelancerDir(exeDir);

    const IniDocument doc = IniParser::parseFile(initialWorldPath);
    QSet<QString> seen;
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Group"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (nickname.isEmpty())
            continue;
        const QString key = nickname.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        bool ok = false;
        const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt(&ok);
        const QString ingameName = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
        values.append(ingameName.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, ingameName));
    }
    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList loadFactionDisplaysWithIds(const QString &gameRoot, const IdsStringTable &ids)
{
    QStringList values;
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString initialWorldPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("initialworld.ini"));

    const IniDocument doc = IniParser::parseFile(initialWorldPath);
    QSet<QString> seen;
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Group"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (nickname.isEmpty())
            continue;
        const QString key = nickname.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        bool ok = false;
        const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt(&ok);
        const QString ingameName = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
        values.append(ingameName.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, ingameName));
    }
    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList loadPilotNicknames(const QString &gameRoot)
{
    QStringList values;
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString missionsDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS"));
    const QStringList sources = {
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_population.ini")),
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_story.ini")),
    };

    QSet<QString> seen;
    for (const QString &source : sources) {
        if (source.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(source);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Pilot"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty()) {
                continue;
            }
            const QString key = nickname.toLower();
            if (seen.contains(key))
                continue;
            seen.insert(key);
            values.append(nickname);
        }
    }
    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

std::unique_ptr<flatlas::domain::UniverseData> loadUniverseForEditor(const QString &gameRoot)
{
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString universeIni = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    if (universeIni.isEmpty())
        return {};

    auto universe = flatlas::editors::UniverseSerializer::load(universeIni);
    if (!universe)
        return {};

    const QString universeDir = QFileInfo(universeIni).absolutePath();
    for (auto &system : universe->systems) {
        QString absoluteFilePath = flatlas::core::PathUtils::ciResolvePath(universeDir, system.filePath);
        if (absoluteFilePath.isEmpty())
            absoluteFilePath = QDir(universeDir).absoluteFilePath(system.filePath);
        system.filePath = absoluteFilePath;
    }

    return universe;
}

QVector<flatlas::domain::SystemInfo> loadUniverseSystemsForEditor(const QString &gameRoot)
{
    QVector<flatlas::domain::SystemInfo> systems;
    const auto universe = loadUniverseForEditor(gameRoot);
    if (!universe)
        return systems;

    systems = universe->systems;

    std::sort(systems.begin(), systems.end(), [](const flatlas::domain::SystemInfo &lhs,
                                                 const flatlas::domain::SystemInfo &rhs) {
        const QString left = lhs.displayName.trimmed().isEmpty() ? lhs.nickname : lhs.displayName;
        const QString right = rhs.displayName.trimmed().isEmpty() ? rhs.nickname : rhs.displayName;
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    return systems;
}

QString factionNicknameFromDisplay(const QString &raw)
{
    const QString value = raw.trimmed();
    const int sep = value.indexOf(QStringLiteral(" - "));
    return sep > 0 ? value.left(sep).trimmed() : value;
}

QStringList loadStarOptions(const QString &gameRoot)
{
    QStringList values{QStringLiteral("med_white_sun")};
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString starArchPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/stararch.ini"));
    if (!starArchPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(starArchPath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("star"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (!nickname.isEmpty())
                values.append(nickname);
        }
    }
    values.removeDuplicates();
    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList lightSourceTypesFromDocument(const IniDocument &extras)
{
    QStringList values{QStringLiteral("DIRECTIONAL"), QStringLiteral("POINT")};
    for (const IniSection &section : extras) {
        if (section.name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) != 0)
            continue;
        const QString value = section.value(QStringLiteral("type")).trimmed().toUpper();
        if (!value.isEmpty())
            values.append(value);
    }
    values.removeDuplicates();
    return values;
}

QStringList lightAttenCurvesFromDocument(const IniDocument &extras)
{
    QStringList values{QStringLiteral("DYNAMIC_DIRECTION")};
    for (const IniSection &section : extras) {
        if (section.name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) != 0)
            continue;
        const QString value = section.value(QStringLiteral("atten_curve")).trimmed();
        if (!value.isEmpty())
            values.append(value);
    }
    values.removeDuplicates();
    return values;
}

bool parseIniVector3(const QString &raw, QVector3D *outValue)
{
    if (!outValue)
        return false;

    const QStringList parts = raw.split(',', Qt::KeepEmptyParts);
    if (parts.size() < 3)
        return false;

    bool okX = false;
    bool okY = false;
    bool okZ = false;
    const float x = parts[0].trimmed().toFloat(&okX);
    const float y = parts[1].trimmed().toFloat(&okY);
    const float z = parts[2].trimmed().toFloat(&okZ);
    if (!okX || !okY || !okZ)
        return false;

    *outValue = QVector3D(x, y, z);
    return true;
}

QString suggestIndexedNickname(const QString &prefix, const QStringList &existing, int width = 3)
{
    int number = 1;
    while (true) {
        const QString candidate = QStringLiteral("%1%2").arg(prefix).arg(number, width, 10, QLatin1Char('0'));
        if (!existing.contains(candidate, Qt::CaseInsensitive))
            return candidate;
        ++number;
    }
}

QString buoyNicknameSystemToken(const QString &systemFilePath)
{
    QString token = QFileInfo(systemFilePath).completeBaseName().trimmed();
    if (token.isEmpty())
        token = QStringLiteral("System");
    token = token.toLower();
    if (!token.isEmpty())
        token[0] = token.at(0).toUpper();
    return token;
}

QString nextIndexedNickname(const QString &prefix, QSet<QString> *usedNicknames, int width = 3)
{
    if (!usedNicknames)
        return QStringLiteral("%1001").arg(prefix);

    int number = 1;
    while (true) {
        const QString candidate = QStringLiteral("%1%2").arg(prefix).arg(number, width, 10, QLatin1Char('0'));
        const QString key = candidate.toLower();
        if (!usedNicknames->contains(key)) {
            usedNicknames->insert(key);
            return candidate;
        }
        ++number;
    }
}

int buoyIdsNameForArchetype(const QString &archetype)
{
    const QString normalized = archetype.trimmed().toLower();
    if (normalized == QLatin1String("nav_buoy"))
        return 261162;
    if (normalized == QLatin1String("hazard_buoy"))
        return 261163;
    return 0;
}

int buoyIdsInfoForArchetype(const QString &archetype)
{
    const QString normalized = archetype.trimmed().toLower();
    if (normalized == QLatin1String("nav_buoy"))
        return 66147;
    if (normalized == QLatin1String("hazard_buoy"))
        return 66144;
    return 0;
}

int derivedBuoyCountForLine(qreal lineLengthScene, int spacingMeters)
{
    const qreal spacingScene = std::max<qreal>(static_cast<qreal>(spacingMeters) / 100.0, 1.0);
    return std::max(2, qRound(lineLengthScene / spacingScene) + 1);
}

double derivedBuoySpacingMetersForLine(qreal lineLengthScene, int count)
{
    if (count <= 1)
        return 0.0;
    const double lineLengthMeters = static_cast<double>(lineLengthScene) * 100.0;
    return std::max(lineLengthMeters / static_cast<double>(count - 1), 1.0);
}

int nextTradeLaneRingNumber(flatlas::domain::SystemDocument *document, const QString &systemNickname)
{
    if (!document)
        return 1;

    const QString prefix = QStringLiteral("%1_Trade_Lane_Ring_").arg(systemNickname).toLower();
    int maxNumber = 0;
    for (const auto &obj : document->objects()) {
        if (!obj)
            continue;
        const QString nickname = obj->nickname().trimmed();
        if (!nickname.toLower().startsWith(prefix))
            continue;
        bool ok = false;
        const int number = nickname.mid(prefix.size()).toInt(&ok);
        if (ok)
            maxNumber = std::max(maxNumber, number);
    }
    return maxNumber + 1;
}

double tradeLaneYawDegrees(const QPointF &startScenePos, const QPointF &endScenePos)
{
    const double dx = static_cast<double>(endScenePos.x() - startScenePos.x());
    const double dz = static_cast<double>(endScenePos.y() - startScenePos.y());
    double angleDeg = qRadiansToDegrees(std::atan2(dx, dz)) + 180.0;
    if (angleDeg > 180.0)
        angleDeg -= 360.0;
    return angleDeg;
}

bool resolveIdsStringInput(const QString &gameRoot,
                           const QString &rawValue,
                           int currentGlobalId,
                           const QString &currentText,
                           int *outId,
                           QString *errorMessage)
{
    if (!outId)
        return false;

    const QString trimmed = rawValue.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("0")) {
        *outId = 0;
        return true;
    }

    bool ok = false;
    const int numericValue = trimmed.toInt(&ok);
    if (ok && numericValue >= 0) {
        *outId = numericValue;
        return true;
    }

    if (currentGlobalId > 0 && trimmed == currentText.trimmed()) {
        *outId = currentGlobalId;
        return true;
    }

    const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
    const QString targetDll = flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
    int rewriteId = 0;
    if (currentGlobalId > 0) {
        const auto it = std::find_if(dataset.entries.cbegin(), dataset.entries.cend(),
                                     [currentGlobalId](const flatlas::infrastructure::IdsEntryRecord &entry) {
                                         return entry.globalId == currentGlobalId;
                                     });
        if (it != dataset.entries.cend() && it->editable
            && it->dllName.compare(targetDll, Qt::CaseInsensitive) == 0) {
            rewriteId = currentGlobalId;
        }
    }
    QString idsError;
    int newGlobalId = 0;
    if (!flatlas::infrastructure::IdsDataService::writeStringEntry(
            dataset, targetDll, rewriteId, trimmed, &newGlobalId, &idsError)) {
        if (errorMessage) {
            *errorMessage = idsError.trimmed().isEmpty()
                ? QObject::tr("Der IDS-Text konnte nicht geschrieben werden.")
                : idsError;
        }
        return false;
    }

    *outId = newGlobalId;
    return true;
}

QString idsDisplayTextFromTable(const IdsStringTable &ids, int globalId)
{
    return globalId > 0 ? ids.getString(globalId).trimmed() : QString();
}

const IdsStringTable &selectionDisplayIdsTable(const QString &gameRoot)
{
    static QString cachedGameRoot;
    static IdsStringTable cachedIds;

    if (cachedGameRoot == gameRoot)
        return cachedIds;

    cachedGameRoot = gameRoot;
    cachedIds = IdsStringTable();

    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    if (!exeDir.isEmpty())
        cachedIds.loadFromFreelancerDir(exeDir);

    return cachedIds;
}

int idsNameFromRawEntries(const QVector<QPair<QString, QString>> &entries)
{
    for (int index = entries.size() - 1; index >= 0; --index) {
        if (entries[index].first.compare(QStringLiteral("ids_name"), Qt::CaseInsensitive) != 0)
            continue;
        bool ok = false;
        const int value = entries[index].second.trimmed().toInt(&ok);
        if (ok && value > 0)
            return value;
    }
    return 0;
}

QString nicknameWithIngameName(const QString &nickname, int idsName, const QString &gameRoot)
{
    const QString trimmedNickname = nickname.trimmed();
    if (trimmedNickname.isEmpty())
        return {};

    const QString ingameName = idsDisplayTextFromTable(selectionDisplayIdsTable(gameRoot), idsName);
    return ingameName.isEmpty()
        ? trimmedNickname
        : QStringLiteral("%1 - %2").arg(trimmedNickname, ingameName);
}

QString ingameNameForIds(int idsName, const QString &gameRoot)
{
    return idsDisplayTextFromTable(selectionDisplayIdsTable(gameRoot), idsName);
}

QString objectIngameName(const SolarObject &obj, const QString &gameRoot)
{
    const int idsName = obj.idsName() > 0 ? obj.idsName() : idsNameFromRawEntries(obj.rawEntries());
    return ingameNameForIds(idsName, gameRoot);
}

QString zoneIngameName(const ZoneItem &zone, const QString &gameRoot)
{
    return ingameNameForIds(idsNameFromRawEntries(zone.rawEntries()), gameRoot);
}

QString objectHeaderDisplayTitle(const SolarObject &obj, const QString &gameRoot)
{
    const int idsName = obj.idsName() > 0 ? obj.idsName() : idsNameFromRawEntries(obj.rawEntries());
    return nicknameWithIngameName(obj.nickname(), idsName, gameRoot);
}

QString zoneHeaderDisplayTitle(const ZoneItem &zone, const QString &gameRoot)
{
    return nicknameWithIngameName(zone.nickname(), idsNameFromRawEntries(zone.rawEntries()), gameRoot);
}

std::shared_ptr<SolarObject> cloneSolarObject(const std::shared_ptr<SolarObject> &source)
{
    if (!source)
        return {};

    auto clone = std::make_shared<SolarObject>();
    clone->setNickname(source->nickname());
    clone->setArchetype(source->archetype());
    clone->setPosition(source->position());
    clone->setRotation(source->rotation());
    clone->setType(source->type());
    clone->setBase(source->base());
    clone->setDockWith(source->dockWith());
    clone->setGotoTarget(source->gotoTarget());
    clone->setLoadout(source->loadout());
    clone->setComment(source->comment());
    clone->setIdsName(source->idsName());
    clone->setIdsInfo(source->idsInfo());
    clone->setRawEntries(source->rawEntries());
    return clone;
}

std::shared_ptr<ZoneItem> cloneZoneItem(const std::shared_ptr<ZoneItem> &source)
{
    if (!source)
        return {};

    auto clone = std::make_shared<ZoneItem>();
    clone->setNickname(source->nickname());
    clone->setPosition(source->position());
    clone->setSize(source->size());
    clone->setShape(source->shape());
    clone->setRotation(source->rotation());
    clone->setZoneType(source->zoneType());
    clone->setUsage(source->usage());
    clone->setPopType(source->popType());
    clone->setPathLabel(source->pathLabel());
    clone->setComment(source->comment());
    clone->setTightnessXyz(source->tightnessXyz());
    clone->setDamage(source->damage());
    clone->setInterference(source->interference());
    clone->setDragScale(source->dragScale());
    clone->setSortKey(source->sortKey());
    clone->setRawEntries(source->rawEntries());
    return clone;
}

void updateRawNicknameEntry(QVector<QPair<QString, QString>> *entries, const QString &nickname)
{
    if (!entries)
        return;
    bool updated = false;
    for (auto &entry : *entries) {
        if (entry.first.compare(QStringLiteral("nickname"), Qt::CaseInsensitive) == 0) {
            entry.second = nickname;
            updated = true;
        }
    }
    if (!updated)
        entries->append({QStringLiteral("nickname"), nickname});
}

void updateRawParentEntries(QVector<QPair<QString, QString>> *entries, const QHash<QString, QString> &renameMap)
{
    if (!entries)
        return;
    for (auto &entry : *entries) {
        if (entry.first.compare(QStringLiteral("parent"), Qt::CaseInsensitive) != 0)
            continue;
        entry.second = renameMap.value(entry.second.trimmed(), entry.second);
    }
}

float normalizedYawDegrees(float degrees)
{
    float yaw = std::fmod(degrees, 360.0f);
    if (yaw < 0.0f)
        yaw += 360.0f;
    return yaw;
}

QVector<QPair<QString, QString>> removeRawEntriesByKey(const QVector<QPair<QString, QString>> &entries,
                                                       const QString &key)
{
    QVector<QPair<QString, QString>> filtered;
    filtered.reserve(entries.size());
    for (const auto &entry : entries) {
        if (entry.first.compare(key, Qt::CaseInsensitive) != 0)
            filtered.append(entry);
    }
    return filtered;
}

struct TradeLaneDialogCatalog {
    QString gameRoot;
    QStringList archetypes;
    QStringList loadouts;
    QStringList factions;
    QStringList pilots;
    QSet<QString> factionDisplays;
    IdsStringTable ids;
};

const TradeLaneDialogCatalog &tradeLaneDialogCatalog(const QString &gameRoot)
{
    static QString cachedGameRoot;
    static TradeLaneDialogCatalog cachedCatalog;

    if (cachedGameRoot == gameRoot)
        return cachedCatalog;

    cachedGameRoot = gameRoot;
    cachedCatalog = {};
    cachedCatalog.gameRoot = gameRoot;

    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    if (!exeDir.isEmpty())
        cachedCatalog.ids.loadFromFreelancerDir(exeDir);

    cachedCatalog.archetypes = loadSolarArchetypesMatching(gameRoot, {QStringLiteral("trade_lane_ring")});
    cachedCatalog.loadouts = loadLoadoutsMatching(gameRoot, {QStringLiteral("trade_lane_ring")});
    cachedCatalog.factions = loadFactionDisplaysWithIds(gameRoot, cachedCatalog.ids);
    cachedCatalog.pilots = loadPilotNicknames(gameRoot);
    cachedCatalog.factionDisplays = QSet<QString>(cachedCatalog.factions.cbegin(), cachedCatalog.factions.cend());
    return cachedCatalog;
}

QStringList collectEncounterNicknames(flatlas::domain::SystemDocument *document, const QString &gameRoot)
{
    QStringList values;
    QSet<QString> seen;

    if (document) {
        const IniDocument extras = SystemPersistence::extraSections(document);
        for (const IniSection &section : extras) {
            if (section.name.compare(QStringLiteral("EncounterParameters"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;
            const QString key = nickname.toLower();
            if (seen.contains(key))
                continue;
            seen.insert(key);
            values.append(nickname);
        }
    }

    const QString encountersDir = flatlas::core::PathUtils::ciResolvePath(
        flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA")),
        QStringLiteral("MISSIONS/encounters"));
    if (!encountersDir.isEmpty()) {
        QDirIterator it(encountersDir, QStringList{QStringLiteral("*.ini")}, QDir::Files);
        while (it.hasNext()) {
            const QString filePath = it.next();
            const QString nickname = QFileInfo(filePath).completeBaseName().trimmed();
            if (nickname.isEmpty())
                continue;
            const QString key = nickname.toLower();
            if (seen.contains(key))
                continue;
            seen.insert(key);
            values.append(nickname);
        }
    }

    std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList patrolDensityRestrictionsForUsage(const QString &usage)
{
    QStringList values{QStringLiteral("1, patroller"),
                       QStringLiteral("1, police_patroller"),
                       QStringLiteral("1, pirate_patroller")};
    if (usage.trimmed().compare(QStringLiteral("trade"), Qt::CaseInsensitive) != 0) {
        values.append(QStringLiteral("4, lawfuls"));
        values.append(QStringLiteral("4, unlawfuls"));
    }
    return values;
}

bool ensureEncounterParameterExists(flatlas::domain::SystemDocument *document,
                                    const QString &encounterNickname,
                                    const QString &gameRoot,
                                    QString *errorMessage)
{
    if (!document)
        return false;

    const QString nickname = encounterNickname.trimmed();
    if (nickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Es wurde kein Encounter ausgewaehlt.");
        return false;
    }

    IniDocument extras = SystemPersistence::extraSections(document);
    for (const IniSection &section : extras) {
        if (section.name.compare(QStringLiteral("EncounterParameters"), Qt::CaseInsensitive) != 0)
            continue;
        if (section.value(QStringLiteral("nickname")).trimmed().compare(nickname, Qt::CaseInsensitive) == 0)
            return true;
    }

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString encounterRelPath = QStringLiteral("MISSIONS/encounters/%1.ini").arg(nickname);
    const QString absolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, encounterRelPath);
    if (absolutePath.isEmpty() || !QFileInfo::exists(absolutePath)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Fuer den Encounter '%1' wurde keine passende missions/encounters-Datei gefunden.")
                                .arg(nickname);
        }
        return false;
    }

    IniSection section;
    section.name = QStringLiteral("EncounterParameters");
    section.entries = {
        {QStringLiteral("nickname"), nickname},
        {QStringLiteral("filename"), QStringLiteral("missions\\encounters\\%1.ini").arg(nickname)},
    };
    extras.append(section);
    SystemPersistence::setExtraSections(document, extras);
    document->setDirty(true);
    return true;
}

double patrolYawDegrees(const QPointF &startFl, const QPointF &endFl)
{
    const double dx = endFl.x() - startFl.x();
    const double dz = endFl.y() - startFl.y();
    const double axisAngle = qRadiansToDegrees(std::atan2(dz, dx));
    double yaw = 90.0 - axisAngle;
    while (yaw > 180.0)
        yaw -= 360.0;
    while (yaw < -180.0)
        yaw += 360.0;
    return yaw;
}

QPolygonF orientedRectPolygon(const QPointF &start, const QPointF &end, qreal halfWidth)
{
    const QLineF axis(start, end);
    const qreal length = axis.length();
    if (length <= 0.001)
        return QPolygonF();

    const qreal nx = -(axis.dy() / length);
    const qreal ny = axis.dx() / length;
    const QPointF offset(nx * halfWidth, ny * halfWidth);

    QPolygonF polygon;
    polygon << (start + offset)
            << (end + offset)
            << (end - offset)
            << (start - offset);
    return polygon;
}

qreal pointLineDistance(const QPointF &point, const QPointF &start, const QPointF &end)
{
    const QLineF axis(start, end);
    const qreal length = axis.length();
    if (length <= 0.001)
        return 0.0;

    const qreal numerator = std::abs((end.y() - start.y()) * point.x()
                                     - (end.x() - start.x()) * point.y()
                                     + end.x() * start.y()
                                     - end.y() * start.x());
    return numerator / length;
}

QPushButton *makeSidebarButton(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setStyleSheet(QStringLiteral("text-align:left; padding:5px 10px;"));
    return button;
}

SystemDisplayFilterRule makeDefaultDisplayRule(const QString &id,
                                               const QString &name,
                                               DisplayFilterTarget target,
                                               const QString &pattern)
{
    SystemDisplayFilterRule rule;
    rule.id = id;
    rule.name = name;
    rule.enabled = true;
    rule.target = target;
    rule.field = DisplayFilterField::Nickname;
    rule.matchMode = DisplayFilterMatchMode::Contains;
    rule.action = DisplayFilterAction::Hide;
    rule.pattern = pattern;
    return rule;
}

SystemDisplayFilterSettings defaultDisplayFilterSettings()
{
    SystemDisplayFilterSettings settings;
    settings.rules = {
        makeDefaultDisplayRule(QStringLiteral("default-hide-tradelane-label"),
                               QStringLiteral("HIDE Trade_Lane_Ring LABEL"),
                               DisplayFilterTarget::Label,
                               QStringLiteral("Trade_Lane_Ring")),
        makeDefaultDisplayRule(QStringLiteral("default-hide-patrol"),
                               QStringLiteral("HIDE patrol LABEL + ZONE"),
                               DisplayFilterTarget::Both,
                               QStringLiteral("_path_")),
        makeDefaultDisplayRule(QStringLiteral("default-hide-destroy-vignette"),
                               QStringLiteral("Hide destroy_vignette LABEL + ZONE"),
                               DisplayFilterTarget::Both,
                               QStringLiteral("destroy_vignette"))
    };
    return settings;
}

QString rawObjectEntryValue(const SolarObject &obj, const QString &key)
{
    const auto entries = obj.rawEntries();
    for (int index = entries.size() - 1; index >= 0; --index) {
        if (entries[index].first.compare(key, Qt::CaseInsensitive) == 0)
            return entries[index].second.trimmed();
    }
    return {};
}

QString parentNicknameForObject(const SolarObject &obj)
{
    return rawObjectEntryValue(obj, QStringLiteral("parent"));
}

QString normalizedPathKey(const QString &value)
{
    QString normalized = QDir::fromNativeSeparators(value).trimmed();
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

QString normalizedNameToken(const QString &value)
{
    QString token = value.trimmed();
    token.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    token.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    token.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    return token;
}

QString zoneArtTokenFromNickname(const QString &nickname, const QString &fallbackKind, const QString &systemToken)
{
    QString token = normalizedNameToken(nickname).toLower();
    if (token.startsWith(QStringLiteral("zone_")))
        token = token.mid(5);
    const QString systemPrefix = systemToken.toLower() + QLatin1Char('_');
    if (token.startsWith(systemPrefix))
        token = token.mid(systemPrefix.size());
    token.remove(QRegularExpression(QStringLiteral("_\\d+$")));
    token = normalizedNameToken(token).toLower();
    if (token.isEmpty())
        token = normalizedNameToken(fallbackKind).toLower();
    return token;
}

QString suggestedGeneratedFieldFileName(const QString &systemFileStem,
                                        const QString &zoneNickname,
                                        CreateFieldZoneResult::Type type)
{
    const QString systemToken = normalizedNameToken(systemFileStem).toUpper();
    const QString artToken = zoneArtTokenFromNickname(zoneNickname,
                                                      type == CreateFieldZoneResult::Type::Asteroid
                                                          ? QStringLiteral("asteroid")
                                                          : QStringLiteral("nebula"),
                                                      normalizedNameToken(systemFileStem));
    const QRegularExpression suffixPattern(QStringLiteral("_(\\d+)$"));
    const QRegularExpressionMatch suffixMatch = suffixPattern.match(zoneNickname);
    const QString numericSuffix = suffixMatch.hasMatch() ? suffixMatch.captured(1) : QStringLiteral("001");
    return QStringLiteral("%1_%2_%3.ini").arg(systemToken, artToken, numericSuffix);
}

QString normalizedGeneratedZoneTemplateText(const QString &content, const QString &sourceNote)
{
    QString normalized = content;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList rawLines = normalized.split(QLatin1Char('\n'));

    QStringList cleanedLines;
    bool blankPending = false;
    bool skippingExclusionSection = false;
    for (const QString &rawLine : rawLines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed.compare(QStringLiteral("[Exclusion Zones]"), Qt::CaseInsensitive) == 0) {
            skippingExclusionSection = true;
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))
            && skippingExclusionSection) {
            skippingExclusionSection = false;
        }
        if (skippingExclusionSection)
            continue;
        if (rawLine.trimmed().isEmpty()) {
            if (!cleanedLines.isEmpty())
                blankPending = true;
            continue;
        }
        if (blankPending && !cleanedLines.isEmpty())
            cleanedLines.append(QString());
        blankPending = false;
        cleanedLines.append(rawLine.trimmed());
    }

    while (!cleanedLines.isEmpty() && cleanedLines.last().trimmed().isEmpty())
        cleanedLines.removeLast();

    const QString trimmedSource = sourceNote.trimmed();
    if (!trimmedSource.isEmpty()) {
        if (!cleanedLines.isEmpty())
            cleanedLines.append(QString());
        cleanedLines.append(trimmedSource);
    }

    return cleanedLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

const QHash<QString, QString> &solarArchetypeModelPathsForPreview()
{
    static QString cachedGamePath;
    static QHash<QString, QString> cache;

    const QString gamePath = normalizedPathKey(flatlas::core::EditingContext::instance().primaryGamePath());
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

    const IniDocument document = IniParser::parseFile(solarArchPath);
    for (const auto &section : document) {
        const QString nickname = normalizedPathKey(section.value(QStringLiteral("nickname")));
        const QString relativeModelPath = section.value(QStringLiteral("DA_archetype")).trimmed();
        if (nickname.isEmpty() || relativeModelPath.isEmpty())
            continue;

        const QString modelPath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativeModelPath);
        if (!modelPath.isEmpty())
            cache.insert(nickname, modelPath);
    }

    return cache;
}

QQuaternion quaternionFromFreelancerRotation(const QVector3D &rotation)
{
    return QQuaternion::fromEulerAngles(rotation.x(), rotation.y(), rotation.z());
}

QString solarObjectTypeLabel(SolarObject::Type type)
{
    const QMetaEnum typeEnum = QMetaEnum::fromType<SolarObject::Type>();
    const char *enumKey = typeEnum.valueToKey(type);
    return enumKey ? QString::fromLatin1(enumKey) : QObject::tr("Other");
}

class SystemGroupPreviewDialog final : public QDialog {
public:
    explicit SystemGroupPreviewDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("3D Preview"));
        resize(980, 760);

        auto *layout = new QVBoxLayout(this);
        auto *headerLayout = new QHBoxLayout();

        m_titleLabel = new QLabel(tr("No preview loaded"), this);
        m_titleLabel->setWordWrap(true);
        headerLayout->addWidget(m_titleLabel, 1);

        auto *resetButton = new QPushButton(tr("Reset Camera"), this);
        connect(resetButton, &QPushButton::clicked, this, [this]() {
            if (m_viewport)
                m_viewport->resetView();
        });
        headerLayout->addWidget(resetButton);
        layout->addLayout(headerLayout);

        m_viewport = new ModelViewport3D(this);
        layout->addWidget(m_viewport, 1);
    }

    bool loadModelFile(const QString &filePath, const QString &title, QString *errorMessage = nullptr)
    {
        if (!m_viewport)
            return false;
        m_titleLabel->setText(title);
        return m_viewport->loadModelFile(filePath, errorMessage);
    }

    bool loadModelNode(const flatlas::infrastructure::ModelNode &node,
                       const QString &title,
                       QString *errorMessage = nullptr)
    {
        if (!m_viewport)
            return false;
        m_titleLabel->setText(title);
        return m_viewport->loadModelNode(node, errorMessage);
    }

private:
    QLabel *m_titleLabel = nullptr;
    ModelViewport3D *m_viewport = nullptr;
};

}

SystemEditorPage::SystemEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    applyThemeStyling();
    qApp->installEventFilter(this);
    connect(&flatlas::core::Theme::instance(), &flatlas::core::Theme::themeChanged,
            this, [this](const QString &) { applyThemeStyling(); });
}

SystemEditorPage::~SystemEditorPage()
{
    m_isShuttingDown = true;
    qApp->removeEventFilter(this);
    if (m_mapScene)
        disconnect(m_mapScene, nullptr, this, nullptr);
    if (m_mapView)
        disconnect(m_mapView, nullptr, this, nullptr);
    if (m_objectTree)
        disconnect(m_objectTree, nullptr, this, nullptr);
    if (m_sceneView3D)
        disconnect(m_sceneView3D, nullptr, this, nullptr);

    if (m_document)
        SystemPersistence::clearExtras(m_document.get());
}

void SystemEditorPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupToolBar();
    mainLayout->addWidget(m_toolBar);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftSidebar = new QWidget(this);
    auto *leftSidebarLayout = new QVBoxLayout(leftSidebar);
    leftSidebarLayout->setContentsMargins(0, 0, 0, 0);
    leftSidebarLayout->setSpacing(0);

    m_leftSidebarSplitter = new QSplitter(Qt::Vertical, leftSidebar);
    leftSidebarLayout->addWidget(m_leftSidebarSplitter);

    // Object tree (left top)
    auto *objectListHost = new QWidget(m_leftSidebarSplitter);
    auto *objectListLayout = new QVBoxLayout(objectListHost);
    objectListLayout->setContentsMargins(0, 0, 0, 0);
    objectListLayout->setSpacing(6);

    m_objectSearchEdit = new QLineEdit(objectListHost);
    m_objectSearchEdit->setPlaceholderText(tr("Objekte durchsuchen..."));
    m_objectSearchEdit->setClearButtonEnabled(true);
    objectListLayout->addWidget(m_objectSearchEdit);

    m_objectSearchHintLabel = new QLabel(tr("Keine Objekte gefunden"), objectListHost);
    m_objectSearchHintLabel->setVisible(false);
    m_objectSearchHintLabel->setStyleSheet(QStringLiteral("color:#9ca3af; padding:0 2px 4px 2px;"));
    objectListLayout->addWidget(m_objectSearchHintLabel);

    m_objectTree = new QTreeWidget(objectListHost);
    m_objectTree->setHeaderLabels({tr("Nickname"), tr("Ingame Name")});
    m_objectTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_objectTree->setMinimumWidth(200);
    m_objectTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_objectTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_objectTree->header()->setStretchLastSection(true);
    objectListLayout->addWidget(m_objectTree, 1);
    m_leftSidebarSplitter->addWidget(objectListHost);

    auto *editorHost = new QWidget(m_leftSidebarSplitter);
    auto *editorLayout = new QVBoxLayout(editorHost);
    editorLayout->setContentsMargins(8, 8, 8, 8);
    editorLayout->setSpacing(6);

    auto *iniEditorTitle = new QLabel(tr("Objekt-Editor"), editorHost);
    iniEditorTitle->setStyleSheet(QStringLiteral("font-size:18px; font-weight:600;"));
    editorLayout->addWidget(iniEditorTitle);

    m_editorStack = new QStackedWidget(editorHost);
    editorLayout->addWidget(m_editorStack, 1);
    m_emptyEditorPage = new QWidget(m_editorStack);
    auto *emptyLayout = new QVBoxLayout(m_emptyEditorPage);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(6);
    m_emptyEditorLabel = new QLabel(tr("Wähle links ein Objekt oder eine Zone aus."), m_emptyEditorPage);
    m_emptyEditorLabel->setWordWrap(true);
    m_emptyEditorLabel->setStyleSheet(QStringLiteral("color:#9ca3af; padding:6px 2px;"));
    emptyLayout->addWidget(m_emptyEditorLabel);
    emptyLayout->addStretch(1);
    m_editorStack->addWidget(m_emptyEditorPage);

    m_singleEditorPage = new QWidget(m_editorStack);
    auto *singleEditorLayout = new QVBoxLayout(m_singleEditorPage);
    singleEditorLayout->setContentsMargins(0, 0, 0, 0);
    singleEditorLayout->setSpacing(6);
    auto *iniEditorHost = editorHost;
    auto *iniEditorLayout = singleEditorLayout;
    m_iniEditor = new IniCodeEditor(editorHost);
    m_iniEditorHighlighter = new IniSyntaxHighlighter(m_iniEditor->document());
    m_iniEditor->setPlaceholderText(tr("Wähle links ein Objekt oder eine Zone aus."));
    m_iniEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_iniEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    iniEditorLayout->addWidget(m_iniEditor, 1);

    auto *rotationRow = new QHBoxLayout();
    m_rotateLeftButton = new QPushButton(tr("Yaw -15°"), iniEditorHost);
    m_rotateRightButton = new QPushButton(tr("Yaw +15°"), iniEditorHost);
    rotationRow->addWidget(m_rotateLeftButton);
    rotationRow->addWidget(m_rotateRightButton);
    iniEditorLayout->addLayout(rotationRow);

    m_applyIniButton = new QPushButton(tr("Übernehmen"), iniEditorHost);
    iniEditorLayout->addWidget(m_applyIniButton);

    m_openSystemIniButton = new QPushButton(tr("System ini öffnen"), iniEditorHost);
    iniEditorLayout->addWidget(m_openSystemIniButton);

    m_preview3DButton = new QPushButton(tr("3D Preview"), iniEditorHost);
    m_preview3DButton->setEnabled(false);
    iniEditorLayout->addWidget(m_preview3DButton);
    m_editorStack->addWidget(m_singleEditorPage);

    m_multiSelectionPage = new QWidget(m_editorStack);
    auto *multiLayout = new QVBoxLayout(m_multiSelectionPage);
    multiLayout->setContentsMargins(0, 0, 0, 0);
    multiLayout->setSpacing(6);
    m_multiSelectionLabel = new QLabel(tr("Mehrfachauswahl"), m_multiSelectionPage);
    m_multiSelectionLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
    multiLayout->addWidget(m_multiSelectionLabel);
    m_multiSelectionScrollArea = new QScrollArea(m_multiSelectionPage);
    m_multiSelectionScrollArea->setWidgetResizable(true);
    m_multiSelectionScrollArea->setFrameShape(QFrame::NoFrame);
    m_multiSelectionListHost = new QWidget(m_multiSelectionScrollArea);
    m_multiSelectionListLayout = new QVBoxLayout(m_multiSelectionListHost);
    m_multiSelectionListLayout->setContentsMargins(0, 0, 0, 0);
    m_multiSelectionListLayout->setSpacing(6);
    m_multiSelectionListLayout->addStretch(1);
    m_multiSelectionScrollArea->setWidget(m_multiSelectionListHost);
    multiLayout->addWidget(m_multiSelectionScrollArea, 1);
    m_editorStack->addWidget(m_multiSelectionPage);

    m_leftSidebarSplitter->addWidget(iniEditorHost);
    m_leftSidebarSplitter->setStretchFactor(0, 1);
    m_leftSidebarSplitter->setStretchFactor(1, 1);
    m_leftSidebarSplitter->setSizes({360, 420});

    m_splitter->addWidget(leftSidebar);

    // Map view (center)
    m_mapScene = new MapScene(this);
    m_mapView = new SystemMapView(this);
    m_mapView->setFocusPolicy(Qt::StrongFocus);
    m_mapView->setMapScene(m_mapScene);
    m_mapView->setBackgroundPixmap(QPixmap(flatlas::core::Theme::instance().wallpaperResourcePath()),
                                   palette().color(QPalette::Base));
    loadDisplayFilterSettings();
    m_mapView->setDisplayFilterSettings(m_displayFilterSettings);
    m_splitter->addWidget(m_mapView);

    setupRightSidebar();
    m_splitter->addWidget(m_rightSidebar);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({220, 1000, 340});

    mainLayout->addWidget(m_splitter);

    setupShortcutActions();
    connectSignals();
    updateSelectionSummary();
    updateSidebarButtons();
}

void SystemEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(16, 16));

    auto *displayFiltersAction = m_toolBar->addAction(tr("Display Filters"));
    connect(displayFiltersAction, &QAction::triggered, this, &SystemEditorPage::openDisplayFilterDialog);

    auto *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);

    m_selectToolAction = m_toolBar->addAction(tr("SELECT"));
    m_selectToolAction->setCheckable(true);
    toolGroup->addAction(m_selectToolAction);
    connect(m_selectToolAction, &QAction::triggered, this, [this]() { setCurrentEditorTool(EditorTool::Selection); });

    m_moveToolAction = m_toolBar->addAction(tr("MOVE"));
    m_moveToolAction->setCheckable(true);
    toolGroup->addAction(m_moveToolAction);
    connect(m_moveToolAction, &QAction::triggered, this, [this]() { setCurrentEditorTool(EditorTool::Move); });

    m_rotateToolAction = m_toolBar->addAction(tr("ROTATE"));
    m_rotateToolAction->setCheckable(true);
    toolGroup->addAction(m_rotateToolAction);
    connect(m_rotateToolAction, &QAction::triggered, this, [this]() { setCurrentEditorTool(EditorTool::Rotate); });

    m_scaleToolAction = m_toolBar->addAction(tr("SCALE"));
    m_scaleToolAction->setCheckable(true);
    toolGroup->addAction(m_scaleToolAction);
    connect(m_scaleToolAction, &QAction::triggered, this, [this]() { setCurrentEditorTool(EditorTool::Scale); });

    m_toolBar->addSeparator();

    m_focusSelectionAction = m_toolBar->addAction(tr("Focus"));
    connect(m_focusSelectionAction, &QAction::triggered, this, &SystemEditorPage::focusCurrentSelectionInView);

    auto *zoomFitAction = m_toolBar->addAction(tr("Zoom Fit"));
    connect(zoomFitAction, &QAction::triggered, this, [this]() {
        if (m_mapView)
            m_mapView->zoomToFit();
    });

    m_toggleGridAction = m_toolBar->addAction(tr("Grid"));
    m_toggleGridAction->setCheckable(true);
    m_toggleGridAction->setChecked(true);
    connect(m_toggleGridAction, &QAction::toggled, this, [this](bool checked) {
        if (m_mapScene)
            m_mapScene->setGridVisible(checked);
    });

    setCurrentEditorTool(EditorTool::Selection);
}

void SystemEditorPage::setupShortcutActions()
{
    auto registerAction = [this](QAction *action, const QList<QKeySequence> &shortcuts) {
        if (!action)
            return;
        action->setShortcuts(shortcuts);
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        addAction(action);
    };

    auto registerShortcutProxy = [this, &registerAction](const QList<QKeySequence> &shortcuts,
                                                         const std::function<void()> &handler) {
        auto *action = new QAction(this);
        connect(action, &QAction::triggered, this, [handler]() { handler(); });
        registerAction(action, shortcuts);
    };

    registerShortcutProxy({QKeySequence(Qt::Key_Q), QKeySequence(Qt::Key_V)}, [this]() {
        if (canTriggerEditorShortcut(true) && m_selectToolAction)
            m_selectToolAction->trigger();
    });
    registerShortcutProxy({QKeySequence(Qt::Key_W)}, [this]() {
        if (canTriggerEditorShortcut(true) && m_moveToolAction)
            m_moveToolAction->trigger();
    });
    registerShortcutProxy({QKeySequence(Qt::Key_E)}, [this]() {
        if (canTriggerEditorShortcut(true) && m_rotateToolAction)
            m_rotateToolAction->trigger();
    });
    registerShortcutProxy({QKeySequence(Qt::Key_R)}, [this]() {
        if (canTriggerEditorShortcut(true) && m_scaleToolAction)
            m_scaleToolAction->trigger();
    });
    registerShortcutProxy({QKeySequence(Qt::Key_G)}, [this]() {
        if (canTriggerEditorShortcut(true) && m_toggleGridAction)
            m_toggleGridAction->trigger();
    });
    registerShortcutProxy({QKeySequence(Qt::Key_F)}, [this]() {
        if (canTriggerEditorShortcut(true) && m_focusSelectionAction)
            m_focusSelectionAction->trigger();
    });

    m_deleteSelectionAction = new QAction(this);
    connect(m_deleteSelectionAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            onDeleteSelected();
    });
    registerAction(m_deleteSelectionAction, {QKeySequence(Qt::Key_Delete)});

    m_duplicateSelectionAction = new QAction(this);
    connect(m_duplicateSelectionAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            onDuplicateSelected();
    });
    registerAction(m_duplicateSelectionAction, {QKeySequence(Qt::CTRL | Qt::Key_D)});

    m_copySelectionAction = new QAction(this);
    connect(m_copySelectionAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            copySelectedToClipboard();
    });
    registerAction(m_copySelectionAction, {QKeySequence::Copy});

    m_pasteSelectionAction = new QAction(this);
    connect(m_pasteSelectionAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            pasteClipboardSelection();
    });
    registerAction(m_pasteSelectionAction, {QKeySequence::Paste});

    m_undoAction = new QAction(this);
    connect(m_undoAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            flatlas::core::UndoManager::instance().undo();
    });
    registerAction(m_undoAction, {QKeySequence::Undo});

    m_redoAction = new QAction(this);
    connect(m_redoAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            flatlas::core::UndoManager::instance().redo();
    });
    registerAction(m_redoAction, {QKeySequence::Redo,
                                  QKeySequence(Qt::CTRL | Qt::Key_Y),
                                  QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});

    m_selectAllAction = new QAction(this);
    connect(m_selectAllAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            selectAllEligibleEntries();
    });
    registerAction(m_selectAllAction, {QKeySequence::SelectAll});

    m_saveAction = new QAction(this);
    connect(m_saveAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut())
            save();
    });
    registerAction(m_saveAction, {QKeySequence::Save});

    m_cancelAction = new QAction(this);
    connect(m_cancelAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            cancelCurrentEditorInteraction();
    });
    registerAction(m_cancelAction, {QKeySequence(Qt::Key_Escape)});

    m_moveLeftAction = new QAction(this);
    connect(m_moveLeftAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(-100.0f, 0.0f, 0.0f), tr("Nudge Selection"));
    });
    registerAction(m_moveLeftAction, {QKeySequence(Qt::Key_Left)});

    m_moveRightAction = new QAction(this);
    connect(m_moveRightAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(100.0f, 0.0f, 0.0f), tr("Nudge Selection"));
    });
    registerAction(m_moveRightAction, {QKeySequence(Qt::Key_Right)});

    m_moveUpAction = new QAction(this);
    connect(m_moveUpAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(0.0f, 0.0f, -100.0f), tr("Nudge Selection"));
    });
    registerAction(m_moveUpAction, {QKeySequence(Qt::Key_Up)});

    m_moveDownAction = new QAction(this);
    connect(m_moveDownAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(0.0f, 0.0f, 100.0f), tr("Nudge Selection"));
    });
    registerAction(m_moveDownAction, {QKeySequence(Qt::Key_Down)});

    m_moveLeftLargeAction = new QAction(this);
    connect(m_moveLeftLargeAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(-1000.0f, 0.0f, 0.0f), tr("Move Selection"));
    });
    registerAction(m_moveLeftLargeAction, {QKeySequence(Qt::SHIFT | Qt::Key_Left)});

    m_moveRightLargeAction = new QAction(this);
    connect(m_moveRightLargeAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(1000.0f, 0.0f, 0.0f), tr("Move Selection"));
    });
    registerAction(m_moveRightLargeAction, {QKeySequence(Qt::SHIFT | Qt::Key_Right)});

    m_moveUpLargeAction = new QAction(this);
    connect(m_moveUpLargeAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(0.0f, 0.0f, -1000.0f), tr("Move Selection"));
    });
    registerAction(m_moveUpLargeAction, {QKeySequence(Qt::SHIFT | Qt::Key_Up)});

    m_moveDownLargeAction = new QAction(this);
    connect(m_moveDownLargeAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            moveSelectedEntries(QVector3D(0.0f, 0.0f, 1000.0f), tr("Move Selection"));
    });
    registerAction(m_moveDownLargeAction, {QKeySequence(Qt::SHIFT | Qt::Key_Down)});

    m_rotateLeftAction = new QAction(this);
    connect(m_rotateLeftAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            rotateSelectedEntriesYaw(-15.0f, tr("Rotate Selection"));
    });
    registerAction(m_rotateLeftAction, {QKeySequence(Qt::Key_Z)});

    m_rotateRightAction = new QAction(this);
    connect(m_rotateRightAction, &QAction::triggered, this, [this]() {
        if (canTriggerEditorShortcut(true))
            rotateSelectedEntriesYaw(15.0f, tr("Rotate Selection"));
    });
    registerAction(m_rotateRightAction, {QKeySequence(Qt::Key_X)});
}

void SystemEditorPage::connectSignals()
{
    connect(m_mapScene, &MapScene::selectionNicknamesChanged, this, &SystemEditorPage::onCanvasSelectionChanged);
    connect(m_applyIniButton, &QPushButton::clicked, this, &SystemEditorPage::applyIniEditorChanges);
    connect(m_rotateLeftButton, &QPushButton::clicked, this, [this]() { rotateSelectedObjectYaw(-15.0f); });
    connect(m_rotateRightButton, &QPushButton::clicked, this, [this]() { rotateSelectedObjectYaw(15.0f); });
    connect(m_openSystemIniButton, &QPushButton::clicked, this, &SystemEditorPage::openSystemIniExternally);
    connect(m_preview3DButton, &QPushButton::clicked, this, &SystemEditorPage::open3DPreviewForSelection);
    connect(m_mapView, &SystemMapView::itemsMoved, this, &SystemEditorPage::onItemsMoved);
    connect(m_mapView, &SystemMapView::itemsMoveStarted, this, &SystemEditorPage::onItemsMoveStarted);
    connect(m_mapView, &SystemMapView::itemsMoving, this, &SystemEditorPage::onItemsMoving);

    // 3D → 2D selection sync

    connect(m_objectTree, &QTreeWidget::itemSelectionChanged,
            this, &SystemEditorPage::onTreeSelectionChanged);
    connect(m_objectSearchEdit, &QLineEdit::textChanged,
            this, [this]() { applyObjectListSearchFilter(); });
        connect(m_iniEditor, &QPlainTextEdit::textChanged,
            this, [this]() { updateSidebarButtons(); });
}

    bool SystemEditorPage::isEditableShortcutTarget(QWidget *widget) const
    {
        if (!widget)
            return false;
        if (qobject_cast<QLineEdit *>(widget))
            return true;
        if (qobject_cast<QAbstractSpinBox *>(widget))
            return true;
        if (qobject_cast<QPlainTextEdit *>(widget))
            return true;
        if (qobject_cast<QTextEdit *>(widget))
            return true;
        if (auto *comboBox = qobject_cast<QComboBox *>(widget))
            return comboBox->isEditable();
        return false;
    }

    bool SystemEditorPage::isMapCanvasFocusWidget(QWidget *widget) const
    {
        return widget && m_mapView && (widget == m_mapView || m_mapView->isAncestorOf(widget));
    }

    bool SystemEditorPage::canTriggerEditorShortcut(bool requiresCanvasFocus) const
    {
        if (!isVisible() || !window() || !window()->isActiveWindow())
            return false;

        QWidget *focusWidget = QApplication::focusWidget();
        if (!focusWidget || (focusWidget != this && !isAncestorOf(focusWidget)))
            return false;
        if (isEditableShortcutTarget(focusWidget))
            return false;
        if (requiresCanvasFocus && !isMapCanvasFocusWidget(focusWidget))
            return false;
        return true;
    }

    bool SystemEditorPage::shouldConsumeShortcutOverride(QKeyEvent *event) const
    {
        if (!event)
            return false;

        QWidget *focusWidget = QApplication::focusWidget();
        if (!focusWidget || (focusWidget != this && !isAncestorOf(focusWidget)))
            return false;
        if (!isEditableShortcutTarget(focusWidget))
            return false;

        const Qt::KeyboardModifiers mods = event->modifiers();
        const int key = event->key();
        if (mods == Qt::NoModifier) {
            switch (key) {
            case Qt::Key_Q:
            case Qt::Key_V:
            case Qt::Key_W:
            case Qt::Key_E:
            case Qt::Key_R:
            case Qt::Key_G:
            case Qt::Key_F:
            case Qt::Key_Escape:
            case Qt::Key_Delete:
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Z:
            case Qt::Key_X:
                return true;
            default:
                return false;
            }
        }

        if (mods == Qt::ShiftModifier) {
            switch (key) {
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Z:
                return true;
            default:
                return false;
            }
        }

        if (mods == Qt::ControlModifier) {
            switch (key) {
            case Qt::Key_A:
            case Qt::Key_C:
            case Qt::Key_D:
            case Qt::Key_S:
            case Qt::Key_V:
            case Qt::Key_Y:
            case Qt::Key_Z:
                return true;
            default:
                return false;
            }
        }

        if (mods == (Qt::ControlModifier | Qt::ShiftModifier))
            return key == Qt::Key_Z;

        return false;
    }

    void SystemEditorPage::setCurrentEditorTool(EditorTool tool)
    {
        m_currentEditorTool = tool;
        if (m_selectToolAction)
            m_selectToolAction->setChecked(tool == EditorTool::Selection);
        if (m_moveToolAction)
            m_moveToolAction->setChecked(tool == EditorTool::Move);
        if (m_rotateToolAction)
            m_rotateToolAction->setChecked(tool == EditorTool::Rotate);
        if (m_scaleToolAction)
            m_scaleToolAction->setChecked(tool == EditorTool::Scale);
        if (m_mapScene)
            m_mapScene->setMoveEnabled(tool == EditorTool::Move);
    }

    QString SystemEditorPage::uniqueNicknameForCopy(const QString &baseNickname) const
    {
        const QString trimmed = baseNickname.trimmed();
        QString candidate = trimmed.endsWith(QStringLiteral("_copy"), Qt::CaseInsensitive)
            ? trimmed
            : trimmed + QStringLiteral("_copy");
        if (!findObjectByNickname(candidate) && !findZoneByNickname(candidate)
            && findLightSourceSectionIndexByNickname(candidate) < 0) {
            return candidate;
        }

        for (int index = 1; index < 10000; ++index) {
            const QString numbered = QStringLiteral("%1_%2").arg(candidate).arg(index, 3, 10, QLatin1Char('0'));
            if (!findObjectByNickname(numbered) && !findZoneByNickname(numbered)
                && findLightSourceSectionIndexByNickname(numbered) < 0) {
                return numbered;
            }
        }
        return candidate + QStringLiteral("_new");
    }

    void SystemEditorPage::focusCurrentSelectionInView()
    {
        if (!m_mapView)
            return;
        if (m_selectedNicknames.isEmpty()) {
            m_mapView->zoomToFit();
        } else {
            m_mapView->focusSelection();
        }
        m_mapView->setFocus();
    }

    void SystemEditorPage::selectAllEligibleEntries()
    {
        if (!m_document || !m_objectTree)
            return;

        QStringList nicknames;
        for (int i = 0; i < m_objectTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *root = m_objectTree->topLevelItem(i);
            if (!root)
                continue;
            for (int childIndex = 0; childIndex < root->childCount(); ++childIndex) {
                QTreeWidgetItem *child = root->child(childIndex);
                if (!child || child->isHidden())
                    continue;
                nicknames.append(child->text(0));
            }
        }
        nicknames = normalizeSelectionNicknames(nicknames);
        if (nicknames.isEmpty())
            return;

        m_selectedNicknames = nicknames;
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateSelectionSummary();
        updateIniEditorForSelection();
        updateSidebarButtons();
    }

    bool SystemEditorPage::cancelCurrentEditorInteraction()
    {
        if (m_pendingFieldZoneRequest) {
            cancelFieldZonePlacement();
            return true;
        }
        if (m_pendingExclusionZoneRequest) {
            cancelExclusionZonePlacement();
            return true;
        }
        if (m_pendingSimpleZoneRequest) {
            cancelSimpleZonePlacement();
            return true;
        }
        if (m_pendingPatrolZoneRequest) {
            cancelPatrolZonePlacement();
            return true;
        }
        if (m_pendingBuoyRequest) {
            cancelBuoyPlacement();
            return true;
        }
        if (m_pendingTradeLaneHasStart) {
            cancelTradeLanePlacement();
            return true;
        }
        if (m_mapView && m_mapView->hasActiveMeasurement()) {
            m_mapView->cancelActiveMeasurement();
            return true;
        }
        if (m_currentEditorTool != EditorTool::Selection) {
            setCurrentEditorTool(EditorTool::Selection);
            return true;
        }
        return false;
    }

    void SystemEditorPage::moveSelectedEntries(const QVector3D &delta, const QString &undoText)
    {
        if (!m_document || m_selectedNicknames.isEmpty())
            return;
        if (m_selectedNicknames.size() == 1 && hasPendingIniEditorChangesForSelection())
            return;

        const QStringList expandedNicknames = expandMoveNicknames(m_selectedNicknames);
        if (expandedNicknames.isEmpty())
            return;

        auto *stack = flatlas::core::UndoManager::instance().stack();
        stack->beginMacro(undoText);
        bool changedAnything = false;

        for (const auto &obj : m_document->objects()) {
            if (!expandedNicknames.contains(obj->nickname()))
                continue;
            const QVector3D oldPos = obj->position();
            const QVector3D newPos = oldPos + delta;
            stack->push(new MoveObjectCommand(obj.get(), oldPos, newPos, undoText));
            changedAnything = true;
        }
        for (const auto &zone : m_document->zones()) {
            if (!expandedNicknames.contains(zone->nickname()))
                continue;
            const QVector3D oldPos = zone->position();
            const QVector3D newPos = oldPos + delta;
            stack->push(new MoveZoneCommand(zone.get(), oldPos, newPos, undoText));
            changedAnything = true;
        }

        stack->endMacro();
        if (!changedAnything)
            return;

        updateIniEditorForSelection();
        updateSidebarButtons();
    }

    void SystemEditorPage::rotateSelectedEntriesYaw(float deltaDegrees, const QString &undoText)
    {
        if (!m_document || m_selectedNicknames.isEmpty())
            return;
        if (m_selectedNicknames.size() == 1 && hasPendingIniEditorChangesForSelection())
            return;

        const QStringList expandedNicknames = expandMoveNicknames(m_selectedNicknames);
        if (expandedNicknames.isEmpty())
            return;

        auto *stack = flatlas::core::UndoManager::instance().stack();
        stack->beginMacro(undoText);
        bool changedAnything = false;

        for (const auto &obj : m_document->objects()) {
            if (!expandedNicknames.contains(obj->nickname()))
                continue;
            const QVector3D oldRotation = obj->rotation();
            QVector3D newRotation = oldRotation;
            newRotation.setY(normalizedYawDegrees(oldRotation.y() + deltaDegrees));
            stack->push(new RotateObjectCommand(obj.get(), oldRotation, newRotation, undoText));
            changedAnything = true;
        }
        for (const auto &zone : m_document->zones()) {
            if (!expandedNicknames.contains(zone->nickname()))
                continue;
            const QVector3D oldRotation = zone->rotation();
            QVector3D newRotation = oldRotation;
            newRotation.setY(normalizedYawDegrees(oldRotation.y() + deltaDegrees));
            stack->push(new RotateZoneCommand(zone.get(), oldRotation, newRotation, undoText));
            changedAnything = true;
        }

        stack->endMacro();
        if (!changedAnything)
            return;

        updateIniEditorForSelection();
        updateSidebarButtons();
    }

    void SystemEditorPage::copySelectedToClipboard()
    {
        m_clipboardPayload.clear();
        m_clipboardPasteSequence = 0;
        if (!m_document || m_selectedNicknames.isEmpty())
            return;

        QSet<QString> copiedObjects;
        QSet<QString> copiedZones;
        for (const QString &nickname : m_selectedNicknames) {
            if (SolarObject *rootObject = findObjectByNickname(nickname)) {
                for (const QString &groupNickname : objectGroupNicknames(rootObject->nickname())) {
                    if (copiedObjects.contains(groupNickname.toLower()))
                        continue;
                    for (const auto &obj : m_document->objects()) {
                        if (obj->nickname() != groupNickname)
                            continue;
                        m_clipboardPayload.objects.append(cloneSolarObject(obj));
                        copiedObjects.insert(groupNickname.toLower());
                        break;
                    }
                }
                continue;
            }
            if (ZoneItem *zone = findZoneByNickname(nickname)) {
                const QString key = zone->nickname().toLower();
                if (copiedZones.contains(key))
                    continue;
                for (const auto &candidate : m_document->zones()) {
                    if (candidate->nickname() != zone->nickname())
                        continue;
                    m_clipboardPayload.zones.append(cloneZoneItem(candidate));
                    copiedZones.insert(key);
                    break;
                }
            }
        }
    }

    void SystemEditorPage::pasteClipboardSelection()
    {
        if (!m_document || m_clipboardPayload.isEmpty())
            return;

        const QVector3D pasteOffset(500.0f * static_cast<float>(m_clipboardPasteSequence + 1), 0.0f, 0.0f);
        QHash<QString, QString> renameMap;
        QStringList newSelectionNicknames;

        for (const auto &source : m_clipboardPayload.objects) {
            if (!source)
                continue;
            renameMap.insert(source->nickname(), uniqueNicknameForCopy(source->nickname()));
        }

        auto *stack = flatlas::core::UndoManager::instance().stack();
        stack->beginMacro(tr("Paste Selection"));
        bool pastedAnything = false;

        for (const auto &source : m_clipboardPayload.objects) {
            if (!source)
                continue;
            auto clone = cloneSolarObject(source);
            const QString newNickname = renameMap.value(source->nickname(), uniqueNicknameForCopy(source->nickname()));
            clone->setNickname(newNickname);
            clone->setPosition(source->position() + pasteOffset);
            QVector<QPair<QString, QString>> entries = clone->rawEntries();
            updateRawNicknameEntry(&entries, newNickname);
            updateRawParentEntries(&entries, renameMap);
            clone->setRawEntries(entries);
            stack->push(new AddObjectCommand(m_document.get(), clone, tr("Paste Selection")));
            newSelectionNicknames.append(newNickname);
            pastedAnything = true;
        }

        for (const auto &source : m_clipboardPayload.zones) {
            if (!source)
                continue;
            auto clone = cloneZoneItem(source);
            const QString newNickname = uniqueNicknameForCopy(source->nickname());
            clone->setNickname(newNickname);
            clone->setPosition(source->position() + pasteOffset);
            QVector<QPair<QString, QString>> entries = clone->rawEntries();
            updateRawNicknameEntry(&entries, newNickname);
            clone->setRawEntries(entries);
            stack->push(new AddZoneCommand(m_document.get(), clone, tr("Paste Selection")));
            newSelectionNicknames.append(newNickname);
            pastedAnything = true;
        }

        stack->endMacro();
        if (!pastedAnything)
            return;

        ++m_clipboardPasteSequence;
        m_selectedNicknames = normalizeSelectionNicknames(newSelectionNicknames);
        refreshObjectList();
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateSelectionSummary();
        updateIniEditorForSelection();
        updateSidebarButtons();
    }

void SystemEditorPage::applyThemeStyling()
{
    const QPalette pal = palette();
    const QColor dim = pal.color(QPalette::PlaceholderText);
    const QColor title = pal.color(QPalette::Text);
    const QColor border = pal.color(QPalette::Mid);

    if (m_objectSearchHintLabel)
        m_objectSearchHintLabel->setStyleSheet(QStringLiteral("color:%1; padding:0 2px 4px 2px;").arg(dim.name()));
    if (m_emptyEditorLabel)
        m_emptyEditorLabel->setStyleSheet(QStringLiteral("color:%1; padding:6px 2px;").arg(dim.name()));
    if (m_multiSelectionLabel)
        m_multiSelectionLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:%1;").arg(title.name()));
    if (m_selectionTitleLabel)
        m_selectionTitleLabel->setStyleSheet(QStringLiteral("font-size:24px; font-weight:700; color:%1;").arg(title.name()));
    if (m_selectionSubtitleLabel)
        m_selectionSubtitleLabel->setStyleSheet(QStringLiteral("color:%1;").arg(dim.name()));
    if (m_deleteSidebarButton) {
        const QColor deleteBg = QColor::fromRgb(160, 46, 46);
        const QColor deleteHover = QColor::fromRgb(184, 58, 58);
        m_deleteSidebarButton->setStyleSheet(
            QStringLiteral("QPushButton { text-align:left; padding:5px 10px; background-color:%1; color:white; font-weight:700;"
                           " border:1px solid %2; border-radius:3px; }"
                           "QPushButton:hover { background-color:%3; }")
                .arg(deleteBg.name(), border.name(), deleteHover.name()));
    }
    updateSaveButtonAppearance();
    refreshSidebarVisibilityState();
    if (m_mapView) {
        m_mapView->setBackgroundPixmap(QPixmap(flatlas::core::Theme::instance().wallpaperResourcePath()),
                                       pal.color(QPalette::Base));
        m_mapView->applyTheme();
    }
}

bool SystemEditorPage::loadFile(const QString &filePath)
{
    emitLoadingProgress(5, tr("Loading system file..."));
    auto doc = SystemPersistence::load(filePath);
    if (!doc)
        return false;

    m_document = std::move(doc);
    loadDocumentIntoUi();
    refreshTitle();
    emitLoadingProgress(100, tr("System loaded"));
    return true;
}

void SystemEditorPage::setDocument(std::unique_ptr<SystemDocument> doc)
{
    if (m_document)
        SystemPersistence::clearExtras(m_document.get());
    m_document = std::move(doc);
    loadDocumentIntoUi();
    refreshTitle();
    emitLoadingProgress(100, tr("System loaded"));
}

void SystemEditorPage::loadDocumentIntoUi()
{
    if (!m_document)
        return;

    cancelFieldZonePlacement();
    cancelExclusionZonePlacement();
    m_pendingGeneratedZoneFiles.clear();
    m_pendingTextFileWrites.clear();
    bindDocumentSignals();
    connectDocumentEntitySignals();
    m_selectedNicknames.clear();
    loadDisplayFilterSettings();
    emitLoadingProgress(20, tr("Preparing 2D system view..."));
    m_mapScene->loadDocument(m_document.get(),
                             [this](int current, int total) {
        const int boundedTotal = std::max(1, total);
        const int percent = 20 + static_cast<int>((static_cast<double>(current) / boundedTotal) * 65.0);
        emitLoadingProgress(percent, tr("Building 2D system view..."));
    });
    syncLightSourcesInScene();
    m_mapView->setSystemName([this]() {
        const QString nickname = m_document->name().trimmed();
        const QString display  = m_document->displayName().trimmed();
        if (!display.isEmpty() && display.compare(nickname, Qt::CaseInsensitive) != 0)
            return QStringLiteral("%1 - %2").arg(nickname, display);
        return nickname;
    }());
    m_mapView->setDisplayFilterSettings(m_displayFilterSettings);
    if (m_is3DViewEnabled) {
        ensureSceneView3D();
        emitLoadingProgress(88, tr("Preparing 3D view..."));
        m_sceneView3D->loadDocument(m_document.get());
    }
    emitLoadingProgress(94, tr("Refreshing object navigation..."));
    refreshObjectList();
    m_mapView->scheduleInitialFit();
    captureSavedDocumentSnapshot();
    refreshDocumentDirtyState();
    emitLoadingProgress(98, tr("Finalizing system view..."));
}

void SystemEditorPage::emitLoadingProgress(int percent, const QString &message)
{
    emit loadingProgressChanged(std::clamp(percent, 0, 100), message);
}

bool SystemEditorPage::save()
{
    if (!m_document || m_document->filePath().isEmpty())
        return false;
    QString generatedFileError;
    if (!writePendingGeneratedZoneFiles(&generatedFileError)) {
        if (!generatedFileError.isEmpty())
            QMessageBox::warning(this, tr("Speichern fehlgeschlagen"), generatedFileError);
        return false;
    }
    QString pendingTextError;
    if (!writePendingTextFiles(&pendingTextError)) {
        if (!pendingTextError.isEmpty())
            QMessageBox::warning(this, tr("Speichern fehlgeschlagen"), pendingTextError);
        return false;
    }
    bool ok = SystemPersistence::save(*m_document);
    if (ok) {
        m_pendingGeneratedZoneFiles.clear();
        m_pendingTextFileWrites.clear();
        captureSavedDocumentSnapshot();
        refreshDocumentDirtyState();
    }
    return ok;
}

bool SystemEditorPage::saveAs(const QString &filePath)
{
    if (!m_document)
        return false;
    m_document->setFilePath(filePath);
    return save();
}

SystemDocument *SystemEditorPage::document() const
{
    return m_document.get();
}

QString SystemEditorPage::filePath() const
{
    return m_document ? m_document->filePath() : QString();
}

bool SystemEditorPage::isDirty() const
{
    return m_document && m_document->isDirty();
}

void SystemEditorPage::bindDocumentSignals()
{
    if (!m_document)
        return;

    connect(m_document.get(), &SystemDocument::dirtyChanged,
            this, &SystemEditorPage::refreshTitle, Qt::UniqueConnection);
    connect(m_document.get(), &SystemDocument::dirtyChanged,
            this, &SystemEditorPage::updateSidebarButtons, Qt::UniqueConnection);
    connect(m_document.get(), &SystemDocument::nameChanged,
            this, &SystemEditorPage::refreshTitle, Qt::UniqueConnection);
}

void SystemEditorPage::refreshTitle()
{
    if (!m_document)
        return;

    QString title = m_document->name().trimmed();
    if (title.isEmpty())
        title = tr("System");
    if (m_document->isDirty())
        title += QLatin1Char('*');
    emit titleChanged(title);
}

void SystemEditorPage::captureSavedDocumentSnapshot()
{
    m_savedDocumentTextSnapshot = m_document ? SystemPersistence::serializeToText(*m_document) : QString();
}

void SystemEditorPage::refreshDocumentDirtyState()
{
    if (!m_document)
        return;

    const bool hasPendingWrites = !m_pendingGeneratedZoneFiles.isEmpty() || !m_pendingTextFileWrites.isEmpty();
    const bool documentChanged = SystemPersistence::serializeToText(*m_document) != m_savedDocumentTextSnapshot;
    m_document->setDirty(hasPendingWrites || documentChanged);
}

void SystemEditorPage::updateSaveButtonAppearance()
{
    if (!m_saveFileButton)
        return;

    const QPalette pal = palette();
    const QColor border = pal.color(QPalette::Mid);
    const QColor cleanBg = pal.color(QPalette::Button);
    const QColor cleanText = pal.color(QPalette::ButtonText);
    const QColor dirtyBg(212, 175, 55);
    const QColor dirtyText(36, 28, 8);
    const bool dirty = m_document && m_document->isDirty();

    m_saveFileButton->setEnabled(dirty);
    m_saveFileButton->setToolTip(dirty
                                     ? tr("Ungespeicherte Änderungen vorhanden.")
                                     : tr("Keine ausstehenden Änderungen zum Speichern."));
    if (dirty) {
        m_saveFileButton->setStyleSheet(
            QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:4px; font-weight:700; padding:6px 10px; }"
                           "QPushButton:hover { background:%4; }"
                           "QPushButton:pressed { background:%5; }")
                .arg(dirtyBg.name(), dirtyText.name(), border.name(), QColor(228, 192, 74).name(), QColor(190, 155, 38).name()));
    } else {
        m_saveFileButton->setStyleSheet(
            QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:4px; font-weight:600; padding:6px 10px; }"
                           "QPushButton:disabled { background:%4; color:%5; border:1px solid %6; }")
                .arg(cleanBg.name(), cleanText.name(), border.name(), pal.color(QPalette::Midlight).name(), pal.color(QPalette::Disabled, QPalette::ButtonText).name(), border.name()));
    }
}

bool SystemEditorPage::hasPendingIniEditorChangesForSelection() const
{
    if (!m_iniEditor || m_selectedNicknames.size() != 1)
        return false;
    return m_iniEditor->toPlainText().trimmed() != serializeSelectionToIni().trimmed();
}

void SystemEditorPage::rotateSelectedObjectYaw(float deltaDegrees)
{
    if (!m_document || m_selectedNicknames.size() != 1 || hasPendingIniEditorChangesForSelection())
        return;

    SolarObject *obj = findObjectByNickname(primarySelectedNickname());
    if (!obj)
        return;

    rotateSelectedEntriesYaw(deltaDegrees, tr("Rotate Selection"));
}

void SystemEditorPage::connectDocumentEntitySignals()
{
    if (!m_document)
        return;

    auto connectObject = [this](const std::shared_ptr<SolarObject> &obj) {
        if (!obj)
            return;
        connect(obj.get(), &SolarObject::changed, this, &SystemEditorPage::refreshDocumentDirtyState, Qt::UniqueConnection);
    };
    auto connectZone = [this](const std::shared_ptr<ZoneItem> &zone) {
        if (!zone)
            return;
        connect(zone.get(), &ZoneItem::changed, this, &SystemEditorPage::refreshDocumentDirtyState, Qt::UniqueConnection);
    };

    for (const auto &obj : m_document->objects())
        connectObject(obj);
    for (const auto &zone : m_document->zones())
        connectZone(zone);

    connect(m_document.get(), &SystemDocument::objectAdded, this,
            [this, connectObject](const std::shared_ptr<SolarObject> &obj) {
                connectObject(obj);
                refreshDocumentDirtyState();
            },
            Qt::UniqueConnection);
    connect(m_document.get(), &SystemDocument::objectRemoved,
            this, &SystemEditorPage::refreshDocumentDirtyState, Qt::UniqueConnection);
    connect(m_document.get(), &SystemDocument::zoneAdded, this,
            [this, connectZone](const std::shared_ptr<ZoneItem> &zone) {
                connectZone(zone);
                refreshDocumentDirtyState();
            },
            Qt::UniqueConnection);
    connect(m_document.get(), &SystemDocument::zoneRemoved,
            this, &SystemEditorPage::refreshDocumentDirtyState, Qt::UniqueConnection);
}

void SystemEditorPage::ensureSceneView3D()
{
    if (m_sceneView3D)
        return;

    m_sceneView3D = new SceneView3D(m_rightSidebar);

    connect(m_sceneView3D, &SceneView3D::objectSelected,
            this, [this](const QString &nickname) {
        onCanvasSelectionChanged({nickname});
    });
}

void SystemEditorPage::set3DViewEnabled(bool enabled)
{
    m_is3DViewEnabled = enabled;

    if (!enabled) {
        return;
    }

    ensureSceneView3D();
    if (m_document)
        m_sceneView3D->loadDocument(m_document.get());
}

void SystemEditorPage::openDisplayFilterDialog()
{
    SystemDisplayFilterDialog dialog(m_displayFilterSettings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_displayFilterSettings = dialog.settings();
    saveDisplayFilterSettings();
    if (m_mapView)
        m_mapView->setDisplayFilterSettings(m_displayFilterSettings);
    pruneSelectionByCurrentFilter();
    refreshSidebarVisibilityState();
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::loadDisplayFilterSettings()
{
    const QJsonObject rawSettings = flatlas::core::Config::instance().getJsonObject(displayFilterConfigKey());
    if (!rawSettings.contains(QStringLiteral("rules"))) {
        m_displayFilterSettings = defaultDisplayFilterSettings();
        return;
    }

    m_displayFilterSettings = flatlas::rendering::SystemDisplayFilterSettings::fromJson(rawSettings);
}

void SystemEditorPage::saveDisplayFilterSettings() const
{
    auto &config = flatlas::core::Config::instance();
    config.setJsonObject(displayFilterConfigKey(), m_displayFilterSettings.toJson());
    config.save();
}

QString SystemEditorPage::displayFilterConfigKey() const
{
    const QString profileId = flatlas::core::EditingContext::instance().editingProfileId();
    return profileId.isEmpty()
        ? QStringLiteral("systemDisplayFilters.default")
        : QStringLiteral("systemDisplayFilters.%1").arg(profileId);
}

void SystemEditorPage::setupRightSidebar()
{
    m_rightSidebar = new QWidget(this);
    m_rightSidebar->setMinimumWidth(320);
    auto *layout = new QVBoxLayout(m_rightSidebar);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_selectionTitleLabel = new QLabel(tr("Kein Objekt ausgewählt"), m_rightSidebar);
    m_selectionTitleLabel->setStyleSheet(QStringLiteral("font-size:24px; font-weight:700;"));
    layout->addWidget(m_selectionTitleLabel);

    m_selectionSubtitleLabel = new QLabel(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."), m_rightSidebar);
    m_selectionSubtitleLabel->setWordWrap(true);
    m_selectionSubtitleLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    layout->addWidget(m_selectionSubtitleLabel);

    auto *groupsHost = new QWidget(m_rightSidebar);
    auto *groupsLayout = new QGridLayout(groupsHost);
    groupsLayout->setContentsMargins(0, 0, 0, 0);
    groupsLayout->setHorizontalSpacing(8);
    groupsLayout->setVerticalSpacing(0);

    auto *creationGroup = new QGroupBox(tr("Erstellung"), groupsHost);
    auto *creationLayout = new QVBoxLayout(creationGroup);
    creationLayout->setContentsMargins(6, 6, 6, 6);
    creationLayout->setSpacing(4);

    m_createObjectButton = makeSidebarButton(tr("Objekt"), creationGroup);
    connect(m_createObjectButton, &QPushButton::clicked, this, &SystemEditorPage::onAddObject);
    creationLayout->addWidget(m_createObjectButton);

    m_createAsteroidNebulaButton = makeSidebarButton(tr("Asteroid / Nebel"), creationGroup);
    connect(m_createAsteroidNebulaButton, &QPushButton::clicked,
            this, &SystemEditorPage::onCreateAsteroidNebulaZone);
    creationLayout->addWidget(m_createAsteroidNebulaButton);

    m_createZoneButton = makeSidebarButton(tr("Zone"), creationGroup);
    connect(m_createZoneButton, &QPushButton::clicked, this, [this]() {
        if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
            return;
        QStringList existingZoneNicknames;
        for (const auto &zone : m_document->zones()) {
            if (zone)
                existingZoneNicknames.append(zone->nickname());
        }
        const QString systemToken = QFileInfo(m_document->filePath()).completeBaseName().toUpper();
        const QString suggested =
            suggestIndexedNickname(QStringLiteral("Zone_%1_zone_").arg(systemToken.isEmpty() ? QStringLiteral("SYSTEM") : systemToken),
                                   existingZoneNicknames);
        CreateSimpleZoneDialog dialog(suggested, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        beginSimpleZonePlacement(dialog.result());
    });
    creationLayout->addWidget(m_createZoneButton);

    m_createPatrolZoneButton = makeSidebarButton(tr("Patrol-Zone"), creationGroup);
    connect(m_createPatrolZoneButton, &QPushButton::clicked, this, &SystemEditorPage::onCreatePatrolZone);
    creationLayout->addWidget(m_createPatrolZoneButton);

    m_createJumpButton = makeSidebarButton(tr("Jump"), creationGroup);
    connect(m_createJumpButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateJumpConnection);
    creationLayout->addWidget(m_createJumpButton);

    m_createSunButton = makeSidebarButton(tr("Sonne"), creationGroup);
    connect(m_createSunButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateSun);
    creationLayout->addWidget(m_createSunButton);

    m_createPlanetButton = makeSidebarButton(tr("Planet"), creationGroup);
    connect(m_createPlanetButton, &QPushButton::clicked, this, &SystemEditorPage::onCreatePlanet);
    creationLayout->addWidget(m_createPlanetButton);

    m_createLightButton = makeSidebarButton(tr("Lichtquelle"), creationGroup);
    connect(m_createLightButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateLightSource);
    creationLayout->addWidget(m_createLightButton);

    m_createWreckButton = makeSidebarButton(tr("Wrack/Surprise"), creationGroup);
    connect(m_createWreckButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateSurprise);
    creationLayout->addWidget(m_createWreckButton);

    m_createBuoyButton = makeSidebarButton(tr("Boje"), creationGroup);
    connect(m_createBuoyButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateBuoy);
    creationLayout->addWidget(m_createBuoyButton);

    m_createWeaponPlatformButton = makeSidebarButton(tr("Waffenplattform"), creationGroup);
    connect(m_createWeaponPlatformButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateWeaponPlatform);
    creationLayout->addWidget(m_createWeaponPlatformButton);

    m_createDepotButton = makeSidebarButton(tr("Depot"), creationGroup);
    connect(m_createDepotButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateDepot);
    creationLayout->addWidget(m_createDepotButton);

    m_createTradelaneButton = makeSidebarButton(tr("Tradelane"), creationGroup);
    connect(m_createTradelaneButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateTradeLane);
    creationLayout->addWidget(m_createTradelaneButton);

    m_createBaseButton = makeSidebarButton(tr("Base"), creationGroup);
    connect(m_createBaseButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateBase);
    creationLayout->addWidget(m_createBaseButton);

    m_createDockingRingButton = makeSidebarButton(tr("Docking Ring"), creationGroup);
    connect(m_createDockingRingButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateDockingRing);
    creationLayout->addWidget(m_createDockingRingButton);

    m_createRingButton = makeSidebarButton(tr("Ring"), creationGroup);
    connect(m_createRingButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateRing);
    creationLayout->addWidget(m_createRingButton);
    creationLayout->addStretch(1);

    auto *editingGroup = new QGroupBox(tr("Bearbeitung"), groupsHost);
    auto *editingLayout = new QVBoxLayout(editingGroup);
    editingLayout->setContentsMargins(6, 6, 6, 6);
    editingLayout->setSpacing(4);

    m_editTradelaneButton = makeSidebarButton(tr("Tradelane"), editingGroup);
    connect(m_editTradelaneButton, &QPushButton::clicked, this, &SystemEditorPage::onEditTradeLane);
    editingLayout->addWidget(m_editTradelaneButton);

    m_editZonePopulationButton = makeSidebarButton(tr("Zone Population"), editingGroup);
    connect(m_editZonePopulationButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Zone Population"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_edit_zone_population"));
    });
    editingLayout->addWidget(m_editZonePopulationButton);

    m_editRingButton = makeSidebarButton(tr("Ring"), editingGroup);
    connect(m_editRingButton, &QPushButton::clicked, this, &SystemEditorPage::onEditRing);
    editingLayout->addWidget(m_editRingButton);

    m_addExclusionZoneButton = makeSidebarButton(tr("Exclusion Zone hinzufügen..."), editingGroup);
    connect(m_addExclusionZoneButton, &QPushButton::clicked,
            this, &SystemEditorPage::onCreateExclusionZone);
    editingLayout->addWidget(m_addExclusionZoneButton);

    m_editBaseButton = makeSidebarButton(tr("Base"), editingGroup);
    connect(m_editBaseButton, &QPushButton::clicked, this, [this]() {
        if (!m_document)
            return;

        SolarObject *hostObject = findBaseHostForSelection();
        if (!hostObject) {
            QMessageBox::information(this,
                                     tr("Base bearbeiten"),
                                     tr("Bitte waehle ein Objekt mit verknuepfter Base aus."));
            return;
        }

        QHash<QString, QString> overrides;
        for (auto it = m_pendingTextFileWrites.constBegin(); it != m_pendingTextFileWrites.constEnd(); ++it)
            overrides.insert(it.key(), it.value().content);

        BaseEditState state;
        QString errorMessage;
        if (!BaseEditService::loadState(*m_document,
                                        *hostObject,
                                        flatlas::core::EditingContext::instance().primaryGamePath(),
                                        overrides,
                                        &state,
                                        &errorMessage)) {
            QMessageBox::warning(this,
                                 tr("Base bearbeiten"),
                                 errorMessage.trimmed().isEmpty()
                                     ? tr("Die Base-Daten konnten nicht geladen werden.")
                                     : errorMessage);
            return;
        }

        BaseEditDialog dialog(state, overrides, this);
        connect(&dialog, &BaseEditDialog::roomActivationRequested, this, [this](const QString &roomName, const QString &modelPath) {
            auto *mainWindow = qobject_cast<MainWindow *>(window());
            if (!mainWindow || !mainWindow->showModelInViewer(modelPath, tr("Room Preview: %1").arg(roomName))) {
                QMessageBox::warning(this,
                                     tr("Room Preview"),
                                     tr("Der ausgewaehlte Room konnte nicht im Haupt-3D-Viewer angezeigt werden."));
            }
        });
        if (dialog.exec() != QDialog::Accepted)
            return;

        BaseApplyResult applyResult;
        if (!BaseEditService::applyEdit(*hostObject,
                                        dialog.state(),
                                        flatlas::core::EditingContext::instance().primaryGamePath(),
                                        overrides,
                                        &applyResult,
                                        &errorMessage)) {
            QMessageBox::warning(this,
                                 tr("Base bearbeiten"),
                                 errorMessage.trimmed().isEmpty()
                                     ? tr("Die Base konnte nicht aktualisiert werden.")
                                     : errorMessage);
            return;
        }

        for (const BaseStagedWrite &write : applyResult.stagedWrites)
            stagePendingTextWrite(write.absolutePath, write.content);

        m_document->setDirty(true);
        refreshObjectList();
        syncTreeSelectionFromNicknames({hostObject->nickname()});
        syncSceneSelectionFromNicknames({hostObject->nickname()});
        updateSelectionSummary();
        updateIniEditorForSelection();
    });
    editingLayout->addWidget(m_editBaseButton);

    m_baseBuilderButton = makeSidebarButton(tr("Base Builder"), editingGroup);
    connect(m_baseBuilderButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Base Builder"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_open_base_builder_for_object"));
    });
    editingLayout->addWidget(m_baseBuilderButton);

    editingLayout->addSpacing(12);
    m_deleteSidebarButton = makeSidebarButton(tr("DELETE"), editingGroup);
    m_deleteSidebarButton->setStyleSheet(QStringLiteral("text-align:left; padding:5px 10px; background-color:#7f1d1d; color:white; font-weight:700;"));
    connect(m_deleteSidebarButton, &QPushButton::clicked, this, &SystemEditorPage::onDeleteSelected);
    editingLayout->addWidget(m_deleteSidebarButton);
    editingLayout->addStretch(1);

    groupsLayout->addWidget(creationGroup, 0, 0);
    groupsLayout->addWidget(editingGroup, 0, 1);
    layout->addWidget(groupsHost);

    m_systemSettingsButton = new QPushButton(tr("System-Einstellungen"), m_rightSidebar);
    m_systemSettingsButton->setMinimumHeight(30);
    connect(m_systemSettingsButton, &QPushButton::clicked, this, &SystemEditorPage::openSystemSettingsDialog);
    layout->addWidget(m_systemSettingsButton);

    auto *jumpRow = new QHBoxLayout();
    m_objectJumpCombo = new QComboBox(m_rightSidebar);
    m_objectJumpCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    jumpRow->addWidget(m_objectJumpCombo, 1);
    m_objectJumpButton = new QPushButton(tr("Springen"), m_rightSidebar);
    connect(m_objectJumpButton, &QPushButton::clicked, this, &SystemEditorPage::jumpToSelectedFromSidebar);
    jumpRow->addWidget(m_objectJumpButton);
    layout->addLayout(jumpRow);

    auto *divider = new QFrame(m_rightSidebar);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    layout->addWidget(divider);

    m_systemFileInfoLabel = new QLabel(m_rightSidebar);
    m_systemFileInfoLabel->setWordWrap(true);
    layout->addWidget(m_systemFileInfoLabel);

    m_systemStatsLabel = new QLabel(m_rightSidebar);
    m_systemStatsLabel->setWordWrap(true);
    layout->addWidget(m_systemStatsLabel);

    m_saveFileButton = new QPushButton(tr("Änderungen in Datei schreiben"), m_rightSidebar);
    m_saveFileButton->setMinimumHeight(34);
    connect(m_saveFileButton, &QPushButton::clicked, this, [this]() {
        if (!save())
            QMessageBox::warning(this, tr("Speichern fehlgeschlagen"), tr("Die Systemdatei konnte nicht gespeichert werden."));
    });
    layout->addWidget(m_saveFileButton);
    layout->addStretch(1);
}

void SystemEditorPage::refreshObjectList()
{
    const QStringList previousSelection = m_selectedNicknames;
    // Clearing and rebuilding the tree emits itemSelectionChanged. Block those
    // signals so onTreeSelectionChanged() cannot fire a scene-selection
    // round-trip while the document is still being loaded into the UI.
    QSignalBlocker treeBlocker(m_objectTree);
    m_objectTree->clear();
    if (!m_document) {
        treeBlocker.unblock();
        updateIniEditorForSelection();
        return;
    }

    QHash<QString, int> groupChildCounts;
    for (const auto &obj : m_document->objects()) {
        if (!obj || !isChildObject(*obj))
            continue;
        const QString rootNickname = normalizeObjectNicknameToGroupRoot(obj->nickname());
        if (!rootNickname.isEmpty() && rootNickname != obj->nickname())
            groupChildCounts[rootNickname] += 1;
    }

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();

    auto *objRoot = new QTreeWidgetItem(m_objectTree, {tr("Objects")});
    objRoot->setFlags(objRoot->flags() & ~Qt::ItemIsSelectable);
    objRoot->setExpanded(true);
    for (const auto &obj : m_document->objects()) {
        if (!obj)
            continue;
        if (isChildObject(*obj))
            continue;
        auto *item = new QTreeWidgetItem(objRoot);
        item->setText(0, obj->nickname());
        item->setText(1, objectIngameName(*obj, gameRoot));
        item->setToolTip(0, obj->nickname());
        item->setToolTip(1, item->text(1));
    }

    auto *zoneRoot = new QTreeWidgetItem(m_objectTree, {tr("Zones")});
    zoneRoot->setFlags(zoneRoot->flags() & ~Qt::ItemIsSelectable);
    zoneRoot->setExpanded(true);
    for (const auto &zone : m_document->zones()) {
        auto *item = new QTreeWidgetItem(zoneRoot);
        item->setText(0, zone->nickname());
        item->setText(1, zoneIngameName(*zone, gameRoot));
        item->setToolTip(0, zone->nickname());
        item->setToolTip(1, item->text(1));
    }

    auto *lightRoot = new QTreeWidgetItem(m_objectTree, {tr("LightSources")});
    lightRoot->setFlags(lightRoot->flags() & ~Qt::ItemIsSelectable);
    lightRoot->setExpanded(true);
    const IniDocument extras = SystemPersistence::extraSections(m_document.get());
    for (const IniSection &section : extras) {
        if (section.name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) != 0)
            continue;
        auto *item = new QTreeWidgetItem(lightRoot);
        const QString lightNickname = section.value(QStringLiteral("nickname")).trimmed();
        const int idsName = idsNameFromRawEntries(section.entries);
        item->setText(0, lightNickname);
        item->setText(1, ingameNameForIds(idsName, gameRoot));
        item->setToolTip(0, lightNickname);
        item->setToolTip(1, item->text(1));
    }

    m_objectTree->resizeColumnToContents(0);
    if (m_objectTree->columnWidth(0) < 260)
        m_objectTree->setColumnWidth(0, 260);

    syncLightSourcesInScene();

    refreshObjectJumpList();
    m_systemFileInfoLabel->setText(QStringLiteral("%1").arg(QFileInfo(m_document->filePath()).fileName()));
    m_systemStatsLabel->setText(tr("Objekte: %1\nZonen: %2")
                                    .arg(m_document->objects().size())
                                    .arg(m_document->zones().size()));
    applyObjectListSearchFilter();
    refreshSidebarVisibilityState();
    // Restore selection with the sync guard active so neither the tree nor the
    // scene emit a round-trip back into this page while the load is still in
    // progress. The tree is already signal-blocked; for the scene we gate with
    // m_syncingSelection which onCanvasSelectionChanged honors as an early
    // return.
    const bool wasSyncing = m_syncingSelection;
    m_syncingSelection = true;
    syncTreeSelectionFromNicknames(previousSelection);
    if (m_mapScene)
        m_mapScene->selectNicknames(expandSelectionNicknamesForScene(previousSelection));
    m_syncingSelection = wasSyncing;
    treeBlocker.unblock();
    updateIniEditorForSelection();
    updateSidebarButtons();
    updateSelectionSummary();
}

void SystemEditorPage::onCanvasSelectionChanged(const QStringList &nicknames)
{
    if (m_isShuttingDown)
        return;
    // Guard against re-entrant notifications triggered by our own programmatic
    // scene-selection updates (group expansion, refresh, etc.). Without this
    // guard, selectNicknames() -> selectionNicknamesChanged -> this slot ->
    // selectNicknames() leads to infinite recursion and a stack overflow.
    if (m_syncingSelection)
        return;
    m_selectedNicknames = normalizeSelectionNicknames(nicknames);
    pruneSelectionByCurrentFilter();
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    // Only push an expanded group selection back to the scene if the scene
    // does not already match the full expanded set. This keeps parent/child
    // highlights in sync without ever re-triggering the same canvas event.
    if (m_mapScene) {
        const QStringList expanded = expandSelectionNicknamesForScene(m_selectedNicknames);
        const QSet<QString> currentScene(nicknames.begin(), nicknames.end());
        const QSet<QString> targetScene(expanded.begin(), expanded.end());
        if (currentScene != targetScene) {
            const bool wasSyncing = m_syncingSelection;
            m_syncingSelection = true;
            m_mapScene->selectNicknames(expanded);
            m_syncingSelection = wasSyncing;
        }
    }

    const QString primaryNickname = primarySelectedNickname();
    if (m_sceneView3D && m_is3DViewEnabled && m_selectedNicknames.size() == 1)
        m_sceneView3D->selectObject(primaryNickname);

    if (m_objectJumpCombo && !primaryNickname.isEmpty())
        m_objectJumpCombo->setCurrentText(primaryNickname);

    if (m_document && m_selectedNicknames.size() == 1) {
        for (const auto &obj : m_document->objects()) {
            if (obj->nickname() == primaryNickname) {
                showObjectProperties(obj.get());
                break;
            }
        }
        for (const auto &zone : m_document->zones()) {
            if (zone->nickname() == primaryNickname) {
                showZoneProperties(zone.get());
                break;
            }
        }
    }

    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::onTreeSelectionChanged()
{
    if (m_isShuttingDown)
        return;
    if (m_syncingSelection)
        return;
    QStringList nicknames;
    const auto selectedItems = m_objectTree->selectedItems();
    nicknames.reserve(selectedItems.size());
    for (QTreeWidgetItem *item : selectedItems) {
        if (item && item->parent())
            nicknames.append(item->text(0));
    }
    nicknames.removeDuplicates();

    m_selectedNicknames = normalizeSelectionNicknames(nicknames);
    pruneSelectionByCurrentFilter();
    syncSceneSelectionFromNicknames(m_selectedNicknames);

    if (m_sceneView3D && m_is3DViewEnabled && m_selectedNicknames.size() == 1)
        m_sceneView3D->selectObject(primarySelectedNickname());

    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::onAddObject()
{
    if (!m_document)
        return;
    if (!m_mapView || !m_mapScene) {
        // Without a live map we cannot run the placement mode, so we would
        // silently fall back to (0,0,0) - better to refuse and surface the
        // issue.
        return;
    }
    if (m_mapView->isPlacementModeActive()) {
        // Guard against re-entering placement while one is still running.
        return;
    }

    CreateObjectDialog dialog(m_document.get(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreateObjectResult result = dialog.result();
    if (result.nickname.isEmpty() || result.archetype.isEmpty())
        return;

    // Helper that maps a freshly-chosen scene point to Freelancer world
    // coordinates and finalizes the object creation. Kept as a single lambda
    // so both map-click and Escape-cancel paths stay consistent.
    auto finalizePlacement = [this, result](const QPointF &scenePos) {
        if (!m_document)
            return;

        const QPointF worldXZ = flatlas::rendering::MapScene::qtToFl(scenePos.x(), scenePos.y());

        // Map archetype → SolarObject::Type heuristically (same rules as
        // SystemPersistence::detectObjectType).
        const QString archetypeLower = result.archetype.toLower();
        SolarObject::Type type = SolarObject::Other;
        if (archetypeLower.contains(QLatin1String("jump_gate")))
            type = SolarObject::JumpGate;
        else if (archetypeLower.contains(QLatin1String("jump_hole")))
            type = SolarObject::JumpHole;
        else if (archetypeLower.contains(QLatin1String("trade")))
            type = SolarObject::TradeLane;
        else if (archetypeLower.contains(QLatin1String("sun")))
            type = SolarObject::Sun;
        else if (archetypeLower.contains(QLatin1String("planet")))
            type = SolarObject::Planet;
        else if (archetypeLower.contains(QLatin1String("satellite")))
            type = SolarObject::Satellite;
        else if (archetypeLower.contains(QLatin1String("waypoint"))
                 || archetypeLower.contains(QLatin1String("buoy")))
            type = SolarObject::Waypoint;
        else if (archetypeLower.contains(QLatin1String("weapons_platform")))
            type = SolarObject::Weapons_Platform;
        else if (archetypeLower.contains(QLatin1String("depot")))
            type = SolarObject::Depot;
        else if (archetypeLower.contains(QLatin1String("dock")))
            type = SolarObject::DockingRing;
        else if (archetypeLower.contains(QLatin1String("wreck")))
            type = SolarObject::Wreck;
        else if (archetypeLower.contains(QLatin1String("asteroid")))
            type = SolarObject::Asteroid;
        else if (archetypeLower.contains(QLatin1String("station")))
            type = SolarObject::Station;

        // Allocate a new ids_name entry for the ingame name via the existing
        // IDS infrastructure. Non-fatal: if the dataset cannot be loaded or
        // the write fails we keep the object but leave ids_name unassigned.
        int assignedIdsName = 0;
        const QString ingameName = result.ingameName.trimmed();
        if (!ingameName.isEmpty()) {
            const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
            if (!gameRoot.isEmpty()) {
                const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
                const QString targetDll =
                    flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
                QString idsError;
                int newGlobalId = 0;
                if (flatlas::infrastructure::IdsDataService::writeStringEntry(
                        dataset, targetDll, 0, ingameName, &newGlobalId, &idsError)) {
                    assignedIdsName = newGlobalId;
                } else if (!idsError.isEmpty()) {
                    QMessageBox::warning(this, tr("IDS-Eintrag"),
                        tr("Ingame-Name konnte nicht als IDS-Eintrag geschrieben werden:\n%1")
                            .arg(idsError));
                }
            }
        }

        auto obj = std::make_shared<SolarObject>();
        obj->setNickname(result.nickname);
        obj->setType(type);
        obj->setArchetype(result.archetype);
        obj->setPosition(QVector3D(static_cast<float>(worldXZ.x()),
                                   0.0f,
                                   static_cast<float>(worldXZ.y())));
        if (!result.loadout.isEmpty())
            obj->setLoadout(result.loadout);
        if (assignedIdsName > 0)
            obj->setIdsName(assignedIdsName);

        QVector<QPair<QString, QString>> rawEntries = obj->rawEntries();
        rawEntries.append({ QStringLiteral("nickname"), result.nickname });
        rawEntries.append({ QStringLiteral("pos"),
                            QStringLiteral("%1, 0, %2")
                                .arg(worldXZ.x(), 0, 'f', 0)
                                .arg(worldXZ.y(), 0, 'f', 0) });
        rawEntries.append({ QStringLiteral("rotate"), QStringLiteral("0, 0, 0") });
        rawEntries.append({ QStringLiteral("Archetype"), result.archetype });
        if (assignedIdsName > 0)
            rawEntries.append({ QStringLiteral("ids_name"), QString::number(assignedIdsName) });
        if (!result.loadout.isEmpty())
            rawEntries.append({ QStringLiteral("loadout"), result.loadout });
        if (!result.reputationNickname.isEmpty())
            rawEntries.append({ QStringLiteral("reputation"), result.reputationNickname });
        obj->setRawEntries(rawEntries);

        auto *cmd = new AddObjectCommand(m_document.get(), obj);
        flatlas::core::UndoManager::instance().push(cmd);
        refreshObjectList();
    };

    // Enter placement mode. A dedicated QObject parent holds the one-shot
    // connections so they get torn down deterministically once either signal
    // fires, and the placementClicked/placementCanceled handlers cannot
    // double-fire even if Qt coalesces events.
    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard, finalizePlacement](const QPointF &scenePos) {
        finalizePlacement(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() {
        placementGuard->deleteLater();
    });

    m_mapView->setPlacementMode(true,
        tr("Klicke auf die Map, um '%1' zu platzieren. [Esc] oder Rechtsklick bricht ab.")
            .arg(result.nickname));
}

void SystemEditorPage::onDeleteSelected()
{
    if (!m_document || m_selectedNicknames.isEmpty())
        return;

    if (m_selectedNicknames.size() == 1) {
        if (SolarObject *jumpObject = findObjectByNickname(m_selectedNicknames.first())) {
            const bool isJumpObject = jumpObject->type() == SolarObject::JumpGate
                                      || jumpObject->type() == SolarObject::JumpHole
                                      || !jumpObject->gotoTarget().trimmed().isEmpty();
            if (isJumpObject) {
                if (!ensureSavedForCrossSystemOperation(tr("Jump-Verbindung loeschen"),
                                                        tr("Jump-Verbindungen werden in beiden Systemdateien direkt entfernt."))) {
                    return;
                }

                const QStringList gotoParts =
                    jumpObject->gotoTarget().split(QLatin1Char(','), Qt::KeepEmptyParts);
                const QString targetSystem = gotoParts.value(0).trimmed();
                const QString targetObject = gotoParts.value(1).trimmed();

                const auto systems =
                    loadUniverseSystemsForEditor(flatlas::core::EditingContext::instance().primaryGamePath());
                QString targetFilePath;
                for (const auto &system : systems) {
                    if (system.nickname.compare(targetSystem, Qt::CaseInsensitive) == 0) {
                        targetFilePath = system.filePath;
                        break;
                    }
                }

                QString prompt = tr("Die Jump-Verbindung '%1' wird aus dem aktuellen System entfernt.")
                                     .arg(jumpObject->nickname());
                bool removeRemoteSide = false;
                if (!targetSystem.isEmpty() && !targetObject.isEmpty()) {
                    removeRemoteSide = true;
                    prompt += tr("\n\nDie Gegenstelle '%1' in System '%2' wird ebenfalls entfernt, damit kein kaputter goto-Link zurueckbleibt.")
                                  .arg(targetObject, targetSystem);
                } else {
                    prompt += tr("\n\nEs wurde keine vollstaendige Gegenstelle gefunden. Es wird nur die lokale Seite entfernt.");
                }

                if (QMessageBox::question(this,
                                          tr("Jump-Verbindung loeschen"),
                                          prompt,
                                          QMessageBox::Yes | QMessageBox::Cancel,
                                          QMessageBox::Cancel) != QMessageBox::Yes) {
                    return;
                }

                JumpConnectionDeleteRequest deleteRequest;
                deleteRequest.currentSystemFilePath = m_document->filePath();
                deleteRequest.currentObjectNickname = jumpObject->nickname();
                deleteRequest.destinationSystemFilePath = targetFilePath;
                deleteRequest.destinationObjectNickname = targetObject;
                deleteRequest.removeRemoteSide = removeRemoteSide && !targetFilePath.trimmed().isEmpty();

                QString errorMessage;
                if (!JumpConnectionService::deleteConnection(deleteRequest, &errorMessage)) {
                    QMessageBox::warning(this, tr("Jump-Verbindung loeschen"),
                                         errorMessage.isEmpty() ? tr("Die Jump-Verbindung konnte nicht geloescht werden.")
                                                                : errorMessage);
                    return;
                }

                loadFile(m_document->filePath());
                return;
            }
        }
    }

    QVector<std::shared_ptr<SolarObject>> objectsToRemove;
    QVector<std::shared_ptr<ZoneItem>> zonesToRemove;
    QStringList lightSourcesToRemove;
    for (const QString &nickname : m_selectedNicknames) {
        if (SolarObject *rootObject = findObjectByNickname(nickname)) {
            for (const QString &groupNickname : objectGroupNicknames(rootObject->nickname())) {
                for (const auto &obj : m_document->objects()) {
                    if (obj->nickname() == groupNickname && !objectsToRemove.contains(obj)) {
                        objectsToRemove.append(obj);
                        break;
                    }
                }
            }
            continue;
        }
        for (const auto &zone : m_document->zones()) {
            if (zone->nickname() == nickname && !zonesToRemove.contains(zone)) {
                zonesToRemove.append(zone);
                break;
            }
        }
        if (findLightSourceSectionIndexByNickname(nickname) >= 0 && !lightSourcesToRemove.contains(nickname, Qt::CaseInsensitive))
            lightSourcesToRemove.append(nickname);
    }

    if (objectsToRemove.isEmpty() && zonesToRemove.isEmpty() && lightSourcesToRemove.isEmpty())
        return;

    if (!zonesToRemove.isEmpty()) {
        QStringList zoneNicknames;
        zoneNicknames.reserve(zonesToRemove.size());
        for (const auto &zone : std::as_const(zonesToRemove)) {
            if (zone) {
                zoneNicknames.append(zone->nickname());
                QString linkedSectionName;
                QString fieldZoneNickname;
                QString relativeFilePath;
                QString absoluteFilePath;
                QString currentText;
                if (resolveLinkedFieldInfoForExclusion(zone->nickname(),
                                                       &linkedSectionName,
                                                       &fieldZoneNickname,
                                                       &relativeFilePath,
                                                       &absoluteFilePath,
                                                       &currentText,
                                                       nullptr)) {
                    const auto patched =
                        ExclusionZoneUtils::patchFieldIniRemoveExclusion(currentText, zone->nickname());
                    if (patched.second)
                        stageFieldIniText(relativeFilePath, absoluteFilePath, patched.first);
                }
            }
        }
        removeLinkedFieldSectionsForNicknames(zoneNicknames);
    }

    auto *stack = flatlas::core::UndoManager::instance().stack();
    stack->beginMacro(tr("Delete Selection"));
    for (const auto &obj : objectsToRemove)
        stack->push(new RemoveObjectCommand(m_document.get(), obj));
    for (const auto &zone : zonesToRemove)
        stack->push(new RemoveZoneCommand(m_document.get(), zone));
    stack->endMacro();

    if (!lightSourcesToRemove.isEmpty()) {
        IniDocument extras = SystemPersistence::extraSections(m_document.get());
        IniDocument filtered;
        filtered.reserve(extras.size());
        for (const IniSection &section : extras) {
            const bool isLightSource = section.name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) == 0;
            const QString lightNickname = section.value(QStringLiteral("nickname")).trimmed();
            if (isLightSource && lightSourcesToRemove.contains(lightNickname, Qt::CaseInsensitive))
                continue;
            filtered.append(section);
        }
        SystemPersistence::setExtraSections(m_document.get(), filtered);
        refreshDocumentDirtyState();
    }

    m_selectedNicknames.clear();
    refreshObjectList();
}

void SystemEditorPage::onDuplicateSelected()
{
    if (!m_document || m_selectedNicknames.isEmpty())
        return;

    const ClipboardPayload savedClipboard = m_clipboardPayload;
    const int savedPasteSequence = m_clipboardPasteSequence;
    copySelectedToClipboard();
    pasteClipboardSelection();
    m_clipboardPayload = savedClipboard;
    m_clipboardPasteSequence = savedPasteSequence;
}

void SystemEditorPage::showObjectProperties(SolarObject *obj)
{
    Q_UNUSED(obj);
    // TODO Phase 5+: Populate PropertiesPanel with object data
}

void SystemEditorPage::showZoneProperties(ZoneItem *zone)
{
    Q_UNUSED(zone);
    // TODO Phase 5+: Populate PropertiesPanel with zone data
}

QString SystemEditorPage::serializeSelectionToIni() const
{
    if (!m_document || m_selectedNicknames.size() != 1)
        return {};
    const QString nickname = m_selectedNicknames.first().trimmed();

    if (SolarObject *rootObject = findObjectByNickname(nickname)) {
        IniDocument doc;
        for (const QString &groupNickname : objectGroupNicknames(rootObject->nickname())) {
            if (SolarObject *obj = findObjectByNickname(groupNickname)) {
                // Live-move: temporarily swap in the dragged position so the
                // ini output reflects the position the user is seeing right
                // now. Signals are blocked so this does not cascade into the
                // scene or other listeners.
                if (m_liveMoveActive && m_liveMoveCurrentWorld.contains(obj->nickname())) {
                    const QVector3D savedPos = obj->position();
                    const QVector3D livePos = m_liveMoveCurrentWorld.value(obj->nickname());
                    {
                        QSignalBlocker blocker(obj);
                        obj->setPosition(livePos);
                        doc.append(SystemPersistence::serializeObjectSection(*obj));
                        obj->setPosition(savedPos);
                    }
                } else {
                    doc.append(SystemPersistence::serializeObjectSection(*obj));
                }
            }
        }
        return IniParser::serialize(doc).trimmed();
    }

    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            IniDocument doc;
            QString leadingComment;
            if (m_liveMoveActive && m_liveMoveCurrentWorld.contains(zone->nickname())) {
                const QVector3D savedPos = zone->position();
                const QVector3D livePos = m_liveMoveCurrentWorld.value(zone->nickname());
                {
                    QSignalBlocker blocker(zone.get());
                    zone->setPosition(livePos);
                    doc.append(SystemPersistence::serializeZoneSection(*zone));
                    leadingComment = zone->comment().trimmed();
                    zone->setPosition(savedPos);
                }
            } else {
                doc.append(SystemPersistence::serializeZoneSection(*zone));
                leadingComment = zone->comment().trimmed();
            }
            QString text = IniParser::serialize(doc).trimmed();
            if (!leadingComment.isEmpty()) {
                const QStringList lines =
                    leadingComment.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
                QStringList commentLines;
                for (const QString &line : lines)
                    commentLines.append(QStringLiteral("; %1").arg(line.trimmed()));
                text = commentLines.join(QLatin1Char('\n')) + QLatin1Char('\n') + text;
            }
            return text;
        }
    }

    const IniDocument extras = SystemPersistence::extraSections(m_document.get());
    for (const IniSection &section : extras) {
        if (section.name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) != 0)
            continue;
        if (section.value(QStringLiteral("nickname")).trimmed().compare(nickname, Qt::CaseInsensitive) != 0)
            continue;
        IniDocument doc;
        doc.append(section);
        return IniParser::serialize(doc).trimmed();
    }

    return {};
}

void SystemEditorPage::updateIniEditorForSelection()
{
    if (m_isShuttingDown)
        return;
    if (!m_editorStack || !m_iniEditor || !m_applyIniButton || !m_openSystemIniButton) {
        return;
    }

    const QString iniText = serializeSelectionToIni();
    const bool hasSelection = !iniText.isEmpty();

    m_iniEditor->setPlainText(hasSelection ? iniText : QString());
    m_iniEditor->setEnabled(hasSelection);
    m_applyIniButton->setEnabled(hasSelection);
    if (!hasSelection) {
        if (m_selectedNicknames.isEmpty())
            m_iniEditor->setPlaceholderText(tr("Wähle links ein Objekt oder eine Zone aus."));
        else
            m_iniEditor->setPlaceholderText(tr("Mehrfachauswahl aktiv. Der Objekt-Editor unterstützt nur genau einen Eintrag."));
    }
    m_openSystemIniButton->setEnabled(m_document && !m_document->filePath().trimmed().isEmpty());
    updateEditorModeUi();
}

void SystemEditorPage::updateEditorModeUi()
{
    if (m_isShuttingDown)
        return;
    if (!m_editorStack)
        return;

    if (m_selectedNicknames.isEmpty()) {
        if (m_emptyEditorLabel)
            m_emptyEditorLabel->setText(tr("Wähle links ein Objekt oder eine Zone aus."));
        m_editorStack->setCurrentWidget(m_emptyEditorPage);
        return;
    }

    if (m_selectedNicknames.size() == 1) {
        m_editorStack->setCurrentWidget(m_singleEditorPage);
        return;
    }

    if (m_multiSelectionLabel) {
        m_multiSelectionLabel->setText(tr("%1 markierte Einträge").arg(m_selectedNicknames.size()));
    }
    rebuildMultiSelectionEditorList();
    m_editorStack->setCurrentWidget(m_multiSelectionPage);
}

void SystemEditorPage::rebuildMultiSelectionEditorList()
{
    if (m_isShuttingDown)
        return;
    if (!m_multiSelectionListLayout)
        return;

    while (m_multiSelectionListLayout->count() > 1) {
        QLayoutItem *item = m_multiSelectionListLayout->takeAt(0);
        if (!item)
            continue;
        if (QWidget *widget = item->widget())
            delete widget;
        delete item;
    }

    for (const QString &nickname : m_selectedNicknames) {
        QString kindText = tr("Objekt");
        if (m_document) {
            for (const auto &zone : m_document->zones()) {
                if (zone->nickname() == nickname) {
                    kindText = tr("Zone");
                    break;
                }
            }
        }
        m_multiSelectionListLayout->insertWidget(m_multiSelectionListLayout->count() - 1,
                                                 buildMultiSelectionRow(nickname, kindText));
    }
}

QWidget *SystemEditorPage::buildMultiSelectionRow(const QString &nickname, const QString &kindText)
{
    const QPalette pal = palette();
    const QColor border = pal.color(QPalette::Mid);
    const QColor bg = pal.color(QPalette::AlternateBase);
    const QColor dim = pal.color(QPalette::PlaceholderText);

    auto *row = new QFrame(m_multiSelectionListHost);
    row->setFrameShape(QFrame::StyledPanel);
    row->setStyleSheet(QStringLiteral("QFrame { border: 1px solid %1; border-radius: 4px; background:%2; }")
                           .arg(border.name(), bg.name()));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    auto *textHost = new QWidget(row);
    auto *textLayout = new QVBoxLayout(textHost);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);
    auto *title = new QLabel(nickname, textHost);
    title->setStyleSheet(QStringLiteral("font-weight:600;"));
    auto *subtitle = new QLabel(kindText, textHost);
    subtitle->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(dim.name()));
    textLayout->addWidget(title);
    textLayout->addWidget(subtitle);
    layout->addWidget(textHost, 1);

    auto *removeButton = new QPushButton(tr("Aus Auswahl"), row);
    removeButton->setMinimumHeight(26);
    connect(removeButton, &QPushButton::clicked, this, [this, nickname]() {
        removeNicknameFromSelection(nickname);
    });
    layout->addWidget(removeButton);

    return row;
}

void SystemEditorPage::removeNicknameFromSelection(const QString &nickname)
{
    if (m_isShuttingDown)
        return;
    m_selectedNicknames.removeAll(normalizeObjectNicknameToGroupRoot(nickname));
    m_selectedNicknames.removeDuplicates();
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::pruneSelectionByCurrentFilter()
{
    if (!m_document)
        return;

    QStringList filtered;
    filtered.reserve(m_selectedNicknames.size());
    for (const QString &nickname : m_selectedNicknames) {
        if (isNicknameVisibleUnderCurrentFilter(nickname))
            filtered.append(nickname);
    }
    filtered.removeDuplicates();
    m_selectedNicknames = filtered;
}

bool SystemEditorPage::isNicknameVisibleUnderCurrentFilter(const QString &nickname) const
{
    if (!m_document)
        return false;

    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname)
            return isObjectVisibleUnderCurrentFilter(*obj);
    }
    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname)
            return isZoneVisibleUnderCurrentFilter(*zone);
    }
    if (findLightSourceSectionIndexByNickname(nickname) >= 0)
        return true;
    return false;
}

bool SystemEditorPage::isObjectVisibleUnderCurrentFilter(const SolarObject &obj) const
{
    SolarObjectDisplayContext context;
    context.nickname = obj.nickname();
    context.archetype = obj.archetype();
    context.type = obj.type();

    bool visible = m_displayFilterSettings.objectVisibleForType(obj.type());
    for (const SystemDisplayFilterRule &rule : m_displayFilterSettings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == DisplayFilterAction::Show);
    }
    return visible;
}

bool SystemEditorPage::isZoneVisibleUnderCurrentFilter(const ZoneItem &zone) const
{
    SolarObjectDisplayContext context;
    context.nickname = zone.nickname();
    context.typeNameOverride = QStringLiteral("Zone");

    bool visible = true;
    for (const SystemDisplayFilterRule &rule : m_displayFilterSettings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == DisplayFilterAction::Show);
    }
    return visible;
}

void SystemEditorPage::applyObjectListSearchFilter()
{
    if (!m_objectTree)
        return;

    const QString needle = m_objectSearchEdit ? m_objectSearchEdit->text().trimmed() : QString();
    const bool hasSearch = !needle.isEmpty();
    int totalVisibleChildren = 0;
    const QColor hitBackground(57, 104, 168, 70);

    for (int i = 0; i < m_objectTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *root = m_objectTree->topLevelItem(i);
        if (!root)
            continue;

        int visibleChildren = 0;
        for (int row = 0; row < root->childCount(); ++row) {
            QTreeWidgetItem *child = root->child(row);
            if (!child)
                continue;

            const bool matches = !hasSearch
                || child->text(0).contains(needle, Qt::CaseInsensitive)
                || child->text(1).contains(needle, Qt::CaseInsensitive);
            child->setHidden(!matches);
            for (int column = 0; column < child->columnCount(); ++column)
                child->setBackground(column, matches && hasSearch ? QBrush(hitBackground) : QBrush());
            if (matches) {
                ++visibleChildren;
                ++totalVisibleChildren;
            }
        }

        root->setHidden(hasSearch && visibleChildren == 0);
        if (!root->isHidden())
            root->setExpanded(true);
    }

    if (m_objectSearchHintLabel)
        m_objectSearchHintLabel->setVisible(hasSearch && totalVisibleChildren == 0);
}

void SystemEditorPage::refreshSidebarVisibilityState()
{
    if (!m_objectTree || !m_document)
        return;

    const QList<QTreeWidgetItem *> matches = m_objectTree->findItems(QStringLiteral("*"),
                                                                     Qt::MatchWildcard | Qt::MatchRecursive,
                                                                     0);
    for (QTreeWidgetItem *item : matches) {
        if (!item->parent())
            continue;

        const QString nickname = item->text(0);
        bool visible = true;
        bool found = false;

        for (const auto &obj : m_document->objects()) {
            if (obj->nickname() == nickname) {
                visible = isObjectVisibleUnderCurrentFilter(*obj);
                found = true;
                break;
            }
        }

        if (!found) {
            for (const auto &zone : m_document->zones()) {
                if (zone->nickname() == nickname) {
                    visible = isZoneVisibleUnderCurrentFilter(*zone);
                    found = true;
                    break;
                }
            }
        }

        applySidebarFilteredStyle(item, visible);
    }
}

void SystemEditorPage::applySidebarFilteredStyle(QTreeWidgetItem *item, bool visible)
{
    if (!item)
        return;

    const QPalette palette = this->palette();
    const QColor normalText = palette.color(QPalette::Active, QPalette::Text);
    const QColor mutedText = QColor(120, 128, 140);

    for (int column = 0; column < item->columnCount(); ++column) {
        item->setForeground(column, visible ? QBrush(normalText) : QBrush(mutedText));
        QFont font = item->font(column);
        font.setItalic(!visible);
        item->setFont(column, font);
    }

    item->setData(0, Qt::UserRole + 1, !visible);
    const QString tooltip = visible
        ? QString()
        : tr("Durch den Sichtbarkeitsfilter aktuell ausgeblendet");
    item->setToolTip(0, tooltip);
    item->setToolTip(1, tooltip);
}

void SystemEditorPage::applyIniEditorChanges()
{
    if (!m_document || !m_iniEditor)
        return;

    const QString text = m_iniEditor->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Leerer Inhalt"),
                             tr("Der Objekt-Editor enthält keine INI-Section."));
        return;
    }

    const IniDocument parsed = IniParser::parseText(text);
    if (parsed.isEmpty()) {
        QMessageBox::warning(this, tr("Ungültige Section"),
                             tr("Bitte mindestens eine gültige INI-Section einfügen."));
        return;
    }

    const IniSection &section = parsed.first();
    const QString sectionName = section.name.trimmed().toLower();
    if (m_selectedNicknames.size() != 1) {
        QMessageBox::information(this, tr("Mehrfachauswahl"),
                                 tr("Der Objekt-Editor kann nur genau einen Eintrag gleichzeitig bearbeiten."));
        return;
    }

    const QString previousNickname = m_selectedNicknames.first();
    SolarObject *selectedRootObject = findObjectByNickname(previousNickname);

    if (sectionName == QStringLiteral("object")) {
        if (!selectedRootObject) {
            QMessageBox::warning(this, tr("Section passt nicht"),
                                 tr("Die Section passt nicht zum aktuell ausgewählten Eintrag."));
            return;
        }

        const QStringList groupNicknames = objectGroupNicknames(selectedRootObject->nickname());
        const QSet<QString> expectedNicknames(groupNicknames.begin(), groupNicknames.end());
        QSet<QString> parsedNicknames;

        for (const IniSection &parsedSection : parsed) {
            if (parsedSection.name.trimmed().compare(QStringLiteral("object"), Qt::CaseInsensitive) != 0) {
                QMessageBox::warning(this, tr("Ungültige Section"),
                                     tr("Bei Parent-/Child-Gruppen dürfen nur [Object]-Sections bearbeitet werden."));
                return;
            }

            const QString parsedNickname = parsedSection.value(QStringLiteral("nickname")).trimmed();
            if (parsedNickname.isEmpty()) {
                QMessageBox::warning(this, tr("Ungültige Section"),
                                     tr("Jede [Object]-Section benötigt einen Nickname."));
                return;
            }
            parsedNicknames.insert(parsedNickname);
        }

        if (parsedNicknames != expectedNicknames) {
            QMessageBox::warning(this, tr("Section passt nicht"),
                                 tr("Die bearbeitete Auswahl muss Parent und alle zugehörigen Child-Objekte vollständig enthalten."));
            return;
        }

        bool changedAnything = false;
        for (const IniSection &parsedSection : parsed) {
            SolarObject *target = findObjectByNickname(parsedSection.value(QStringLiteral("nickname")).trimmed());
            if (!target)
                continue;

            IniDocument beforeDoc;
            beforeDoc.append(SystemPersistence::serializeObjectSection(*target));
            const QString beforeText = IniParser::serialize(beforeDoc).trimmed();
            SystemPersistence::applyObjectSection(*target, parsedSection);
            IniDocument afterDoc;
            afterDoc.append(SystemPersistence::serializeObjectSection(*target));
            const QString afterText = IniParser::serialize(afterDoc).trimmed();
            changedAnything = changedAnything || (beforeText != afterText);
        }

        if (changedAnything)
            m_document->setDirty(true);
        m_selectedNicknames = normalizeSelectionNicknames({selectedRootObject->nickname()});
        refreshObjectList();
        onCanvasSelectionChanged(m_selectedNicknames);
        return;
    }

    if (parsed.size() == 1 && sectionName == QStringLiteral("zone")) {
        for (const auto &zone : m_document->zones()) {
            if (zone->nickname() == previousNickname) {
                IniDocument beforeDoc;
                beforeDoc.append(SystemPersistence::serializeZoneSection(*zone));
                const QString beforeText = IniParser::serialize(beforeDoc).trimmed();
                const QString previousComment = zone->comment();
                bool parsedHasCommentEntry = false;
                for (const auto &entry : section.entries) {
                    if (entry.first.compare(QStringLiteral("comment"), Qt::CaseInsensitive) == 0) {
                        parsedHasCommentEntry = true;
                        break;
                    }
                }
                SystemPersistence::applyZoneSection(*zone, section);
                if (!parsedHasCommentEntry && !previousComment.trimmed().isEmpty())
                    zone->setComment(previousComment);
                IniDocument afterDoc;
                afterDoc.append(SystemPersistence::serializeZoneSection(*zone));
                const QString afterText = IniParser::serialize(afterDoc).trimmed();
                if (beforeText != afterText)
                    m_document->setDirty(true);
                m_selectedNicknames = {zone->nickname()};
                refreshObjectList();
                onCanvasSelectionChanged(m_selectedNicknames);
                return;
            }
        }
    }

    if (parsed.size() == 1 && sectionName == QStringLiteral("lightsource")) {
        const int index = findLightSourceSectionIndexByNickname(previousNickname);
        if (index >= 0) {
            IniDocument extras = SystemPersistence::extraSections(m_document.get());
            const QString parsedNickname = section.value(QStringLiteral("nickname")).trimmed();
            if (parsedNickname.isEmpty()) {
                QMessageBox::warning(this, tr("Ungueltige Section"),
                                     tr("Die LightSource-Section benoetigt einen Nickname."));
                return;
            }
            if (parsedNickname.compare(previousNickname, Qt::CaseInsensitive) != 0
                && (findObjectByNickname(parsedNickname)
                    || findZoneByNickname(parsedNickname)
                    || findLightSourceSectionIndexByNickname(parsedNickname) >= 0)) {
                QMessageBox::warning(this, tr("Ungueltige Section"),
                                     tr("Der Nickname kollidiert mit einem vorhandenen Eintrag."));
                return;
            }
            extras[index] = section;
            SystemPersistence::setExtraSections(m_document.get(), extras);
            refreshDocumentDirtyState();
            m_selectedNicknames = {parsedNickname};
            refreshObjectList();
            syncTreeSelectionFromNicknames(m_selectedNicknames);
            updateSelectionSummary();
            updateIniEditorForSelection();
            return;
        }
    }

    QMessageBox::warning(this, tr("Section passt nicht"),
                         tr("Die Section passt nicht zum aktuell ausgewählten Eintrag."));
}

void SystemEditorPage::openSystemIniExternally() const
{
    if (!m_document || m_document->filePath().trimmed().isEmpty())
        return;

    QDesktopServices::openUrl(QUrl::fromLocalFile(m_document->filePath()));
}

void SystemEditorPage::updateSelectionSummary()
{
    if (m_isShuttingDown)
        return;
    if (!m_selectionTitleLabel || !m_selectionSubtitleLabel || !m_document) {
        if (m_selectionTitleLabel)
            m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
        if (m_selectionSubtitleLabel)
            m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
        emit selectionStatusChanged(tr("0 Objekte markiert"));
        return;
    }

    if (m_selectedNicknames.isEmpty()) {
        m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
        m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
        emit selectionStatusChanged(tr("0 Objekte markiert"));
        return;
    }

    if (m_selectedNicknames.size() > 1) {
        m_selectionTitleLabel->setText(tr("%1 Objekte markiert").arg(m_selectedNicknames.size()));
        m_selectionSubtitleLabel->setText(tr("Mehrfachauswahl aktiv. Bearbeiten und Löschen wirken auf die aktuelle Auswahl."));
        emit selectionStatusChanged(tr("%1 Objekte markiert").arg(m_selectedNicknames.size()));
        return;
    }

    const QString nickname = m_selectedNicknames.first();
    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    if (SolarObject *obj = findObjectByNickname(nickname)) {
        m_selectionTitleLabel->setText(objectHeaderDisplayTitle(*obj, gameRoot));
        const int groupCount = objectGroupNicknames(obj->nickname()).size();
        QString subtitle = tr("Objekt · %1").arg(solarObjectTypeLabel(obj->type()));
        if (groupCount > 1)
            subtitle += tr(" · %1 parts").arg(groupCount);
        m_selectionSubtitleLabel->setText(subtitle);
        emit selectionStatusChanged(groupCount > 1 ? tr("1 Objektgruppe markiert") : tr("1 Objekt markiert"));
        return;
    }

    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            m_selectionTitleLabel->setText(zoneHeaderDisplayTitle(*zone, gameRoot));
            const QString zoneType = zone->zoneType().trimmed().isEmpty() ? tr("Zone") : zone->zoneType().trimmed();
            m_selectionSubtitleLabel->setText(tr("Zone · %1").arg(zoneType));
            emit selectionStatusChanged(tr("1 Objekt markiert"));
            return;
        }
    }

    if (findLightSourceSectionIndexByNickname(nickname) >= 0) {
        m_selectionTitleLabel->setText(nickname);
        m_selectionSubtitleLabel->setText(tr("LightSource"));
        emit selectionStatusChanged(tr("1 Objekt markiert"));
        return;
    }

    m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
    m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
    emit selectionStatusChanged(tr("0 Objekte markiert"));
}

void SystemEditorPage::updateSidebarButtons()
{
    if (m_isShuttingDown)
        return;
    const QString selectedNickname = primarySelectedNickname();
    const bool hasSingleSelection = m_selectedNicknames.size() == 1;
    ZoneItem *selectedZone = nullptr;
    bool isObject = false;
    bool isZone = false;
    SolarObject::Type objectType = SolarObject::Other;

    if (m_document && hasSingleSelection) {
        if (SolarObject *obj = findObjectByNickname(selectedNickname)) {
            isObject = true;
            objectType = obj->type();
        }
        if (!isObject) {
            for (const auto &zone : m_document->zones()) {
                if (zone->nickname() == selectedNickname) {
                    isZone = true;
                    selectedZone = zone.get();
                    break;
                }
            }
        }
    }

    if (m_editTradelaneButton)
        m_editTradelaneButton->setEnabled(hasSingleSelection && isObject && objectType == SolarObject::TradeLane);
    if (m_deleteSidebarButton)
        m_deleteSidebarButton->setEnabled(!m_selectedNicknames.isEmpty());
    if (m_editZonePopulationButton)
        m_editZonePopulationButton->setEnabled(hasSingleSelection && isZone);
    if (m_editRingButton) {
        SolarObject *ringHost = nullptr;
        if (hasSingleSelection) {
            if (isObject) {
                if (SolarObject *obj = findObjectByNickname(selectedNickname)) {
                    if (RingEditService::hasRing(*obj))
                        ringHost = obj;
                }
            } else if (isZone && selectedZone) {
                ringHost = RingEditService::findHostForZone(m_document.get(), selectedZone->nickname());
            }
        }
        m_editRingButton->setEnabled(ringHost != nullptr);
    }
    if (m_addExclusionZoneButton)
        m_addExclusionZoneButton->setEnabled(hasSingleSelection && isZone
                                             && selectedZone
                                             && isFieldZone(*selectedZone));
    if (m_editBaseButton)
        m_editBaseButton->setEnabled(hasSingleSelection && findBaseHostForSelection() != nullptr);
    if (m_baseBuilderButton)
        m_baseBuilderButton->setEnabled(hasSingleSelection && findBaseHostForSelection() != nullptr);
    if (m_saveFileButton)
        updateSaveButtonAppearance();
    if (m_objectJumpButton)
        m_objectJumpButton->setEnabled(m_objectJumpCombo && m_objectJumpCombo->count() > 0);
    if (m_preview3DButton)
        m_preview3DButton->setEnabled(hasSingleObjectGroupSelection());

    const bool hasPendingEditorChanges = hasPendingIniEditorChangesForSelection();
    const bool canRotateObject = hasSingleSelection && isObject && !hasPendingEditorChanges;
    const QString defaultLeftTooltip = tr("Dreht das ausgewählte Objekt um 15 Grad nach links um die Yaw-Achse.");
    const QString defaultRightTooltip = tr("Dreht das ausgewählte Objekt um 15 Grad nach rechts um die Yaw-Achse.");
    const QString blockedTooltip = tr("Bitte zuerst die offenen Änderungen im Objekt-Editor übernehmen, bevor per Button rotiert wird.");
    if (m_rotateLeftButton) {
        m_rotateLeftButton->setEnabled(canRotateObject);
        m_rotateLeftButton->setToolTip(hasPendingEditorChanges ? blockedTooltip : defaultLeftTooltip);
    }
    if (m_rotateRightButton) {
        m_rotateRightButton->setEnabled(canRotateObject);
        m_rotateRightButton->setToolTip(hasPendingEditorChanges ? blockedTooltip : defaultRightTooltip);
    }
}

void SystemEditorPage::refreshObjectJumpList()
{
    if (!m_objectJumpCombo) {
        return;
    }

    m_objectJumpCombo->clear();
    if (!m_document)
        return;

    for (const auto &obj : m_document->objects()) {
        if (!isChildObject(*obj))
            m_objectJumpCombo->addItem(obj->nickname());
    }
    for (const auto &zone : m_document->zones())
        m_objectJumpCombo->addItem(zone->nickname());
}

void SystemEditorPage::jumpToSelectedFromSidebar()
{
    if (!m_mapScene || !m_mapView || !m_objectJumpCombo)
        return;

    const QString nickname = m_objectJumpCombo->currentText().trimmed();
    if (nickname.isEmpty())
        return;
    syncSceneSelectionFromNicknames({nickname});
    for (QGraphicsItem *item : m_mapScene->items()) {
        if (auto *solarItem = dynamic_cast<flatlas::rendering::SolarObjectItem *>(item); solarItem && solarItem->nickname() == nickname) {
            m_mapView->centerOn(solarItem);
            return;
        }
        if (auto *zoneItem = dynamic_cast<flatlas::rendering::ZoneItem2D *>(item); zoneItem && zoneItem->nickname() == nickname) {
            m_mapView->centerOn(zoneItem);
            return;
        }
        if (auto *lightItem = dynamic_cast<flatlas::rendering::LightSourceItem *>(item); lightItem && lightItem->nickname() == nickname) {
            m_mapView->centerOn(lightItem);
            return;
        }
    }
}

bool SystemEditorPage::ensureSavedForCrossSystemOperation(const QString &actionTitle,
                                                          const QString &actionDescription)
{
    if (!m_document)
        return false;
    if (!m_document->isDirty())
        return true;

    const int answer = QMessageBox::question(
        this,
        actionTitle,
        tr("%1\n\nDas aktuelle System hat ungespeicherte Aenderungen. Diese muessen zuerst gespeichert werden, bevor die verknuepfte Cross-System-Operation sicher ausgefuehrt werden kann.")
            .arg(actionDescription),
        QMessageBox::Save | QMessageBox::Cancel,
        QMessageBox::Save);

    if (answer != QMessageBox::Save)
        return false;

    return save();
}

void SystemEditorPage::onItemsMoved(const QHash<QString, QPointF> &oldPositions,
                                    const QHash<QString, QPointF> &newPositions,
                                    double verticalOffsetMeters)
{
    if (!m_document || oldPositions.isEmpty() || newPositions.isEmpty()) {
        // Move ended without a valid payload — clear live-move state.
        m_liveMoveActive = false;
        m_liveMoveStartWorld.clear();
        m_liveMoveCurrentWorld.clear();
        updateIniEditorForSelection();
        return;
    }

    auto *stack = flatlas::core::UndoManager::instance().stack();
    stack->beginMacro(tr("Move Selection"));
    bool movedAnything = false;
    const QStringList expandedNicknames = expandMoveNicknames(oldPositions.keys() + newPositions.keys());

    for (const auto &obj : m_document->objects()) {
        if (!expandedNicknames.contains(obj->nickname()))
            continue;
        QVector3D oldPos = m_liveMoveStartWorld.value(obj->nickname(), obj->position());
        QVector3D newPos = m_liveMoveCurrentWorld.value(obj->nickname(), oldPos);
        if (!m_liveMoveCurrentWorld.contains(obj->nickname())
            && oldPositions.contains(obj->nickname())
            && newPositions.contains(obj->nickname())) {
            const QPointF oldFl = MapScene::qtToFl(oldPositions.value(obj->nickname()).x(), oldPositions.value(obj->nickname()).y());
            const QPointF newFl = MapScene::qtToFl(newPositions.value(obj->nickname()).x(), newPositions.value(obj->nickname()).y());
            const double startY = m_liveMoveStartWorld.contains(obj->nickname())
                                      ? m_liveMoveStartWorld.value(obj->nickname()).y()
                                      : obj->position().y();
            oldPos = QVector3D(oldFl.x(), startY, oldFl.y());
            newPos = QVector3D(newFl.x(), startY + verticalOffsetMeters, newFl.y());
        }
        if (qFuzzyCompare(oldPos.x() + 1.0, newPos.x() + 1.0)
            && qFuzzyCompare(oldPos.y() + 1.0, newPos.y() + 1.0)
            && qFuzzyCompare(oldPos.z() + 1.0, newPos.z() + 1.0))
            continue;
        stack->push(new MoveObjectCommand(obj.get(), oldPos, newPos));
        movedAnything = true;
    }

    for (const auto &zone : m_document->zones()) {
        if (!expandedNicknames.contains(zone->nickname()))
            continue;
        QVector3D oldPos = m_liveMoveStartWorld.value(zone->nickname(), zone->position());
        QVector3D newPos = m_liveMoveCurrentWorld.value(zone->nickname(), oldPos);
        if (!m_liveMoveCurrentWorld.contains(zone->nickname())
            && oldPositions.contains(zone->nickname())
            && newPositions.contains(zone->nickname())) {
            const QPointF oldFl = MapScene::qtToFl(oldPositions.value(zone->nickname()).x(), oldPositions.value(zone->nickname()).y());
            const QPointF newFl = MapScene::qtToFl(newPositions.value(zone->nickname()).x(), newPositions.value(zone->nickname()).y());
            const double startY = m_liveMoveStartWorld.contains(zone->nickname())
                                      ? m_liveMoveStartWorld.value(zone->nickname()).y()
                                      : zone->position().y();
            oldPos = QVector3D(oldFl.x(), startY, oldFl.y());
            newPos = QVector3D(newFl.x(), startY + verticalOffsetMeters, newFl.y());
        }
        if (qFuzzyCompare(oldPos.x() + 1.0, newPos.x() + 1.0)
            && qFuzzyCompare(oldPos.y() + 1.0, newPos.y() + 1.0)
            && qFuzzyCompare(oldPos.z() + 1.0, newPos.z() + 1.0))
            continue;
        stack->push(new MoveZoneCommand(zone.get(), oldPos, newPos));
        movedAnything = true;
    }

    stack->endMacro();

    // Clear the live-move state BEFORE refreshing the ini editor so the
    // refresh reads the final (committed) positions, not the live overrides.
    m_liveMoveActive = false;
    m_liveMoveStartWorld.clear();
    m_liveMoveCurrentWorld.clear();

    if (movedAnything)
        m_document->setDirty(true);
    updateIniEditorForSelection();
}

void SystemEditorPage::onItemsMoveStarted(const QHash<QString, QPointF> &startScenePositions)
{
    if (!m_document)
        return;
    m_liveMoveActive = true;
    m_liveMoveStartWorld.clear();
    m_liveMoveCurrentWorld.clear();
    for (auto it = startScenePositions.constBegin(); it != startScenePositions.constEnd(); ++it) {
        const QString &nickname = it.key();
        if (SolarObject *obj = findObjectByNickname(nickname)) {
            m_liveMoveStartWorld.insert(nickname, obj->position());
            m_liveMoveCurrentWorld.insert(nickname, obj->position());
            continue;
        }
        for (const auto &zone : m_document->zones()) {
            if (zone->nickname() == nickname) {
                m_liveMoveStartWorld.insert(nickname, zone->position());
                m_liveMoveCurrentWorld.insert(nickname, zone->position());
                break;
            }
        }
    }

    const QStringList expandedNicknames = expandMoveNicknames(startScenePositions.keys());
    for (const QString &nickname : expandedNicknames) {
        if (m_liveMoveStartWorld.contains(nickname))
            continue;
        if (SolarObject *obj = findObjectByNickname(nickname)) {
            m_liveMoveStartWorld.insert(nickname, obj->position());
            m_liveMoveCurrentWorld.insert(nickname, obj->position());
            continue;
        }
        if (ZoneItem *zone = findZoneByNickname(nickname)) {
            m_liveMoveStartWorld.insert(nickname, zone->position());
            m_liveMoveCurrentWorld.insert(nickname, zone->position());
        }
    }
}

void SystemEditorPage::onItemsMoving(const QHash<QString, QPointF> &currentScenePositions,
                                     double verticalOffsetMeters)
{
    if (!m_document || !m_liveMoveActive)
        return;

    QHash<QString, QVector3D> rootDeltas;
    for (auto it = currentScenePositions.constBegin(); it != currentScenePositions.constEnd(); ++it) {
        const QString &nickname = it.key();
        if (!m_liveMoveStartWorld.contains(nickname))
            continue;
        const QPointF fl = MapScene::qtToFl(it.value().x(), it.value().y());
        const double startY = m_liveMoveStartWorld.value(nickname).y();
        const QVector3D newPos(static_cast<float>(fl.x()),
                               static_cast<float>(startY + verticalOffsetMeters),
                               static_cast<float>(fl.y()));
        m_liveMoveCurrentWorld.insert(nickname, newPos);
        if (findObjectByNickname(nickname))
            rootDeltas.insert(normalizeObjectNicknameToGroupRoot(nickname), newPos - m_liveMoveStartWorld.value(nickname));
    }

    for (auto it = rootDeltas.constBegin(); it != rootDeltas.constEnd(); ++it) {
        const QStringList linkedNicknames = expandMoveNicknames({it.key()});
        for (const QString &nickname : linkedNicknames) {
            if (!m_liveMoveStartWorld.contains(nickname) || currentScenePositions.contains(nickname))
                continue;
            m_liveMoveCurrentWorld.insert(nickname, m_liveMoveStartWorld.value(nickname) + it.value());
        }
    }
    // Refresh the ini editor so "pos = ..." reflects the live position.
    updateIniEditorForSelection();
}

bool SystemEditorPage::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::ShortcutOverride) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (shouldConsumeShortcutOverride(keyEvent)) {
            event->accept();
            return true;
        }
    }

    if (m_pendingFieldZoneRequest && m_mapView && watched == m_mapView->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updateFieldZonePlacementPreview(m_mapView->mapToScene(mouseEvent->pos()));
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton) {
                cancelFieldZonePlacement();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton && m_pendingFieldZoneHasCenter) {
                finalizeFieldZonePlacement(m_mapView->mapToScene(mouseEvent->pos()));
                return true;
            }
            break;
        }
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelFieldZonePlacement();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (m_pendingExclusionZoneRequest && m_mapView && watched == m_mapView->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updateExclusionZonePlacementPreview(m_mapView->mapToScene(mouseEvent->pos()));
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton) {
                cancelExclusionZonePlacement();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton && m_pendingExclusionZoneHasCenter) {
                finalizeExclusionZonePlacement(m_mapView->mapToScene(mouseEvent->pos()));
                return true;
            }
            break;
        }
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelExclusionZonePlacement();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (m_pendingSimpleZoneRequest && m_mapView && watched == m_mapView->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updateSimpleZonePlacementPreview(m_mapView->mapToScene(mouseEvent->pos()));
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton) {
                cancelSimpleZonePlacement();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton && m_pendingSimpleZoneHasCenter) {
                finalizeSimpleZonePlacement(m_mapView->mapToScene(mouseEvent->pos()));
                return true;
            }
            break;
        }
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelSimpleZonePlacement();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (m_pendingPatrolZoneRequest && m_mapView && watched == m_mapView->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updatePatrolZonePlacementPreview(m_mapView->mapToScene(mouseEvent->pos()));
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton) {
                cancelPatrolZonePlacement();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton && m_pendingPatrolZoneHasStart) {
                const QPointF scenePos = m_mapView->mapToScene(mouseEvent->pos());
                if (m_pendingPatrolZoneStep == 2) {
                    m_pendingPatrolZoneHasEnd = true;
                    m_pendingPatrolZoneEndScenePos = scenePos;
                    m_pendingPatrolZoneStep = 3;
                    if (m_patrolZonePlacementPreview && m_mapScene) {
                        m_mapScene->removeItem(m_patrolZonePlacementPreview);
                        delete m_patrolZonePlacementPreview;
                        m_patrolZonePlacementPreview = nullptr;
                    }
                    updatePatrolZonePlacementPreview(scenePos);
                    m_mapView->setPlacementMode(true,
                                                tr("3. Klick legt die Breite fest. 4. Klick speichert '%1'.")
                                                    .arg(m_pendingPatrolZoneRequest->nickname));
                } else if (m_pendingPatrolZoneStep == 3 && m_pendingPatrolZoneHasEnd) {
                    m_pendingPatrolZoneHalfWidthScene = std::max<qreal>(1.0,
                                                                        pointLineDistance(scenePos,
                                                                                          m_pendingPatrolZoneStartScenePos,
                                                                                          m_pendingPatrolZoneEndScenePos));
                    if (m_patrolZoneWidthPreview) {
                        m_patrolZoneWidthPreview->setPolygon(orientedRectPolygon(m_pendingPatrolZoneStartScenePos,
                                                                                 m_pendingPatrolZoneEndScenePos,
                                                                                 m_pendingPatrolZoneHalfWidthScene));
                    }
                    m_pendingPatrolZoneStep = 4;
                    m_mapView->setPlacementMode(true,
                                                tr("4. Klick speichert '%1'. [Esc] oder Rechtsklick bricht ab.")
                                                    .arg(m_pendingPatrolZoneRequest->nickname));
                } else if (m_pendingPatrolZoneStep == 4 && m_pendingPatrolZoneHasEnd) {
                    finalizePatrolZonePlacement(scenePos);
                }
                return true;
            }
            break;
        }
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelPatrolZonePlacement();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (m_pendingBuoyRequest && m_mapView && watched == m_mapView->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updateBuoyPlacementPreview(m_mapView->mapToScene(mouseEvent->pos()));
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton) {
                cancelBuoyPlacement();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton && m_pendingBuoyHasAnchor) {
                finalizeBuoyPlacement(m_mapView->mapToScene(mouseEvent->pos()));
                return true;
            }
            break;
        }
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelBuoyPlacement();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (m_pendingTradeLaneHasStart && m_mapView && watched == m_mapView->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updateTradeLanePlacementPreview(m_mapView->mapToScene(mouseEvent->pos()));
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton) {
                cancelTradeLanePlacement();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton) {
                finalizeTradeLanePlacement(m_mapView->mapToScene(mouseEvent->pos()));
                return true;
            }
            break;
        }
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelTradeLanePlacement();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SystemEditorPage::syncTreeSelectionFromNicknames(const QStringList &nicknames)
{
    if (!m_objectTree)
        return;

    const QSet<QString> selectedSet(nicknames.begin(), nicknames.end());
    QSignalBlocker blocker(m_objectTree);
    m_objectTree->clearSelection();

    QTreeWidgetItem *firstMatch = nullptr;
    const QList<QTreeWidgetItem *> matches = m_objectTree->findItems(QStringLiteral("*"),
                                                                     Qt::MatchWildcard | Qt::MatchRecursive,
                                                                     0);
    for (QTreeWidgetItem *item : matches) {
        if (!item->parent())
            continue;
        const bool selected = selectedSet.contains(item->text(0));
        item->setSelected(selected);
        if (selected && !firstMatch)
            firstMatch = item;
    }
    m_objectTree->setCurrentItem(firstMatch);
}

void SystemEditorPage::syncSceneSelectionFromNicknames(const QStringList &nicknames)
{
    if (!m_mapScene)
        return;
    // Programmatic scene updates must be invisible to our canvas-selection
    // slot, otherwise the manual selectionNicknamesChanged emission from
    // MapScene::selectNicknames re-enters and recurses.
    const bool wasSyncing = m_syncingSelection;
    m_syncingSelection = true;
    m_mapScene->selectNicknames(expandSelectionNicknamesForScene(nicknames));
    m_syncingSelection = wasSyncing;
}

QString SystemEditorPage::primarySelectedNickname() const
{
    return m_selectedNicknames.isEmpty() ? QString() : m_selectedNicknames.first();
}

void SystemEditorPage::createQuickObject(SolarObject::Type type,
                                         const QString &suggestedNickname,
                                         const QString &defaultArchetype)
{
    if (!m_document)
        return;

    bool ok = false;
    const QString nickname = QInputDialog::getText(this,
                                                   tr("Objekt erstellen"),
                                                   tr("Nickname:"),
                                                   QLineEdit::Normal,
                                                   suggestedNickname,
                                                   &ok).trimmed();
    if (!ok || nickname.isEmpty())
        return;

    auto obj = std::make_shared<SolarObject>();
    obj->setNickname(nickname);
    obj->setType(type);
    obj->setArchetype(defaultArchetype);

    auto *cmd = new AddObjectCommand(m_document.get(), obj, tr("Create Object"));
    flatlas::core::UndoManager::instance().push(cmd);
    refreshObjectList();
}

void SystemEditorPage::showNotYetPorted(const QString &featureName, const QString &v1Hint) const
{
    QString text = tr("%1 ist in V2 noch nicht portiert.").arg(featureName);
    if (!v1Hint.trimmed().isEmpty())
        text += tr("\n\nV1-Referenz: %1").arg(v1Hint);
    QMessageBox::information(const_cast<SystemEditorPage *>(this), tr("Noch nicht portiert"), text);
}

void SystemEditorPage::openSystemSettingsDialog()
{
    if (!m_document)
        return;

    const bool hadDirtyStateBefore = m_document->isDirty();
    const SystemSettingsState currentSettings = SystemSettingsService::load(m_document.get());
    const SystemSettingsOptions options = SystemSettingsService::loadOptions(
        flatlas::core::EditingContext::instance().primaryGamePath());

    SystemSettingsDialog dialog(currentSettings,
                                options,
                                SystemPersistence::hasNonStandardSectionOrder(m_document.get()),
                                this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const SystemSettingsState editedSettings = dialog.result();
    const bool settingsChanged = editedSettings.musicSpace != currentSettings.musicSpace
        || editedSettings.musicDanger != currentSettings.musicDanger
        || editedSettings.musicBattle != currentSettings.musicBattle
        || editedSettings.spaceColor != currentSettings.spaceColor
        || editedSettings.localFaction != SystemSettingsService::factionDisplayForNickname(currentSettings.localFaction,
                                                                                           options.factionDisplayOptions)
        || editedSettings.ambientColor != currentSettings.ambientColor
        || editedSettings.dust != currentSettings.dust
        || editedSettings.backgroundBasicStars != currentSettings.backgroundBasicStars
        || editedSettings.backgroundComplexStars != currentSettings.backgroundComplexStars
        || editedSettings.backgroundNebulae != currentSettings.backgroundNebulae;

    if (dialog.shouldNormalizeSectionOrder() && (hadDirtyStateBefore || settingsChanged)) {
        QMessageBox::warning(this, tr("System-Einstellungen"),
                             tr("Die Section-Reihenfolge kann nur ohne ungespeicherte Aenderungen standardisiert werden.\n"
                                "Bitte speichere oder verwerfe zuerst bestehende Aenderungen und starte die Standardisierung danach separat."));
        return;
    }

    QString errorMessage;
    if (settingsChanged && !SystemSettingsService::apply(m_document.get(), editedSettings, &errorMessage)) {
        QMessageBox::warning(this, tr("System-Einstellungen"),
                             errorMessage.trimmed().isEmpty()
                                 ? tr("Die System-Einstellungen konnten nicht uebernommen werden.")
                                 : errorMessage);
        return;
    }

    if (dialog.shouldNormalizeSectionOrder()) {
        if (m_document->isDirty()) {
            QMessageBox::warning(this, tr("System-Einstellungen"),
                                 tr("Die Section-Reihenfolge kann nur fuer den aktuellen Dateistand auf der Festplatte standardisiert werden.\n"
                                    "Bitte speichere oder verwerfe zuerst ungespeicherte Aenderungen."));
            return;
        }

        bool changed = false;
        QString errorMessage;
        if (!SystemPersistence::normalizeSectionOrderInFile(m_document->filePath(), &changed, &errorMessage)) {
            QMessageBox::warning(this, tr("System-Einstellungen"),
                                 errorMessage.trimmed().isEmpty()
                                     ? tr("Die Section-Reihenfolge konnte nicht standardisiert werden.")
                                     : errorMessage);
            return;
        }

        if (changed)
            loadFile(m_document->filePath());
    }
}

void SystemEditorPage::onCreateSun()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    QStringList existingObjectNicknames;
    for (const auto &obj : m_document->objects()) {
        if (obj)
            existingObjectNicknames.append(obj->nickname());
    }

    const QString systemToken = QFileInfo(m_document->filePath()).completeBaseName().toUpper();
    const QString suggested =
        suggestIndexedNickname(QStringLiteral("%1_sun_").arg(systemToken.isEmpty() ? QStringLiteral("SYSTEM") : systemToken),
                               existingObjectNicknames);
    CreateSunDialog dialog(suggested,
                           loadSolarArchetypesMatching(gameRoot, {QStringLiteral("sun")}),
                           loadStarOptions(gameRoot),
                           this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreateSunRequest request = dialog.result();
    if (request.nickname.isEmpty() || request.archetype.isEmpty())
        return;

    auto finalizePlacement = [this, request](const QPointF &scenePos) {
        if (!m_document)
            return;

        if (findObjectByNickname(request.nickname) || findZoneByNickname(QStringLiteral("Zone_%1_death").arg(request.nickname))) {
            QMessageBox::warning(this, tr("Sonne erstellen"),
                                 tr("Im aktuellen System existiert bereits ein Eintrag mit diesem Nickname."));
            return;
        }

        const QPointF worldXZ = MapScene::qtToFl(scenePos.x(), scenePos.y());
        int assignedIdsName = 0;
        int assignedIdsInfo = 66162;
        const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
        if (!request.ingameName.isEmpty()) {
            const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
            const QString targetDll =
                flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
            QString idsError;
            int newGlobalId = 0;
            if (flatlas::infrastructure::IdsDataService::writeStringEntry(
                    dataset, targetDll, 261008, request.ingameName, &newGlobalId, &idsError)) {
                assignedIdsName = newGlobalId;
            } else {
                QMessageBox::warning(this, tr("Sonne erstellen"),
                                     tr("Der Ingame Name konnte nicht in die IDS-Daten geschrieben werden.\n%1")
                                         .arg(idsError.isEmpty() ? tr("Unbekannter Fehler.") : idsError));
                return;
            }
        }
        if (!request.infoCardText.isEmpty()) {
            const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
            const QString targetDll =
                flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
            QString idsError;
            int newGlobalId = 0;
            if (flatlas::infrastructure::IdsDataService::writeStringEntry(
                    dataset, targetDll, 66162, request.infoCardText, &newGlobalId, &idsError)) {
                assignedIdsInfo = newGlobalId;
            } else {
                QMessageBox::warning(this, tr("Sonne erstellen"),
                                     tr("Der ids_info-Text konnte nicht in die IDS-Daten geschrieben werden.\n%1")
                                         .arg(idsError.isEmpty() ? tr("Unbekannter Fehler.") : idsError));
                return;
            }
        }

        auto sun = std::make_shared<SolarObject>();
        sun->setNickname(request.nickname);
        sun->setType(SolarObject::Sun);
        sun->setArchetype(request.archetype);
        sun->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        sun->setRotation(QVector3D());
        if (assignedIdsName > 0)
            sun->setIdsName(assignedIdsName);
        if (assignedIdsInfo > 0)
            sun->setIdsInfo(assignedIdsInfo);
        QVector<QPair<QString, QString>> objectEntries{
            {QStringLiteral("nickname"), request.nickname},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
            {QStringLiteral("archetype"), request.archetype},
            {QStringLiteral("atmosphere_range"), QString::number(request.atmosphereRange)},
            {QStringLiteral("star"), request.star.isEmpty() ? QStringLiteral("med_white_sun") : request.star},
        };
        if (assignedIdsName > 0)
            objectEntries.append({QStringLiteral("ids_name"), QString::number(assignedIdsName)});
        if (assignedIdsInfo > 0)
            objectEntries.append({QStringLiteral("ids_info"), QString::number(assignedIdsInfo)});
        if (!request.burnColor.isEmpty())
            objectEntries.append({QStringLiteral("burn_color"), request.burnColor});
        sun->setRawEntries(objectEntries);

        auto deathZone = std::make_shared<ZoneItem>();
        deathZone->setNickname(QStringLiteral("Zone_%1_death").arg(request.nickname));
        deathZone->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        deathZone->setRotation(QVector3D());
        deathZone->setShape(ZoneItem::Sphere);
        deathZone->setSize(QVector3D(static_cast<float>(request.deathZoneRadius), 0.0f, 0.0f));
        deathZone->setDamage(request.deathZoneDamage);
        deathZone->setRawEntries({
            {QStringLiteral("nickname"), deathZone->nickname()},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
            {QStringLiteral("shape"), QStringLiteral("SPHERE")},
            {QStringLiteral("size"), QString::number(request.deathZoneRadius)},
            {QStringLiteral("damage"), QString::number(request.deathZoneDamage)},
            {QStringLiteral("ids_info"), QStringLiteral("0")},
        });

        auto *stack = flatlas::core::UndoManager::instance().stack();
        stack->beginMacro(tr("Sonne erstellen"));
        stack->push(new AddObjectCommand(m_document.get(), sun, tr("Create Sun")));
        stack->push(new AddZoneCommand(m_document.get(), deathZone, tr("Create Sun Death Zone")));
        stack->endMacro();
        refreshObjectList();
        m_selectedNicknames = {sun->nickname()};
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateIniEditorForSelection();
    };

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [placementGuard, finalizePlacement](const QPointF &scenePos) {
        finalizePlacement(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() { placementGuard->deleteLater(); });
    m_mapView->setPlacementMode(true,
                                tr("Klicke auf die Map, um '%1' zu platzieren. [Esc] oder Rechtsklick bricht ab.")
                                    .arg(request.nickname));
}

void SystemEditorPage::onCreateLightSource()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    const IniDocument extras = SystemPersistence::extraSections(m_document.get());
    const QString systemToken = QFileInfo(m_document->filePath()).completeBaseName().toUpper();
    const QString suggested =
        suggestIndexedNickname(QStringLiteral("%1_light_").arg(systemToken.isEmpty() ? QStringLiteral("SYSTEM") : systemToken),
                               lightSourceNicknames(),
                               1);
    CreateLightSourceDialog dialog(suggested,
                                   lightSourceTypesFromDocument(extras),
                                   lightAttenCurvesFromDocument(extras),
                                   this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreateLightSourceRequest request = dialog.result();
    if (request.nickname.isEmpty())
        return;

    auto finalizePlacement = [this, request](const QPointF &scenePos) {
        if (!m_document)
            return;
        if (findLightSourceSectionIndexByNickname(request.nickname) >= 0
            || findObjectByNickname(request.nickname)
            || findZoneByNickname(request.nickname)) {
            QMessageBox::warning(this, tr("Lichtquelle erstellen"),
                                 tr("Im aktuellen System existiert bereits ein Eintrag mit diesem Nickname."));
            return;
        }
        const QPointF worldXZ = MapScene::qtToFl(scenePos.x(), scenePos.y());
        IniDocument extras = SystemPersistence::extraSections(m_document.get());
        IniSection section;
        section.name = QStringLiteral("LightSource");
        section.entries = {
            {QStringLiteral("nickname"), request.nickname},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("color"), request.color},
            {QStringLiteral("range"), QString::number(request.range)},
            {QStringLiteral("type"), request.type},
            {QStringLiteral("atten_curve"), request.attenCurve},
        };
        extras.append(section);
        SystemPersistence::setExtraSections(m_document.get(), extras);
        refreshDocumentDirtyState();
        refreshObjectList();
        m_selectedNicknames = {request.nickname};
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        updateSelectionSummary();
        updateIniEditorForSelection();
    };

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [placementGuard, finalizePlacement](const QPointF &scenePos) {
        finalizePlacement(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() { placementGuard->deleteLater(); });
    m_mapView->setPlacementMode(true,
                                tr("Klicke auf die Map, um '%1' zu platzieren. [Esc] oder Rechtsklick bricht ab.")
                                    .arg(request.nickname));
}

void SystemEditorPage::onCreateSurprise()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    QStringList existingObjectNicknames;
    for (const auto &obj : m_document->objects()) {
        if (obj)
            existingObjectNicknames.append(obj->nickname());
    }
    const QString systemToken = QFileInfo(m_document->filePath()).completeBaseName().toUpper();
    const QString suggested =
        suggestIndexedNickname(QStringLiteral("%1_Wreck_").arg(systemToken.isEmpty() ? QStringLiteral("SYSTEM") : systemToken),
                               existingObjectNicknames);
    CreateSurpriseDialog dialog(suggested,
                                loadSolarArchetypesMatching(gameRoot, {QStringLiteral("wreck"),
                                                                       QStringLiteral("surprise"),
                                                                       QStringLiteral("suprise")}),
                                loadLoadoutsWithPrefix(gameRoot, QStringLiteral("SECRET")),
                                this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreateSurpriseRequest request = dialog.result();
    if (request.nickname.isEmpty() || request.archetype.isEmpty())
        return;

    auto finalizePlacement = [this, request](const QPointF &scenePos) {
        if (!m_document)
            return;
        if (findObjectByNickname(request.nickname) || findZoneByNickname(request.nickname)) {
            QMessageBox::warning(this, tr("Surprise erstellen"),
                                 tr("Im aktuellen System existiert bereits ein Eintrag mit diesem Nickname."));
            return;
        }

        const QPointF worldXZ = MapScene::qtToFl(scenePos.x(), scenePos.y());
        int assignedIdsName = 0;
        int assignedIdsInfo = 0;
        if (!request.ingameName.isEmpty()) {
            const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
            const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
            const QString targetDll =
                flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
            QString idsError;
            int newGlobalId = 0;
            if (flatlas::infrastructure::IdsDataService::writeStringEntry(
                    dataset, targetDll, 0, request.ingameName, &newGlobalId, &idsError)) {
                assignedIdsName = newGlobalId;
            } else {
                QMessageBox::warning(this, tr("Surprise erstellen"),
                                     tr("Der Ingame Name konnte nicht in die IDS-Daten geschrieben werden.\n%1")
                                         .arg(idsError.isEmpty() ? tr("Unbekannter Fehler.") : idsError));
                return;
            }
        }
        if (!request.infoCardText.isEmpty()) {
            const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
            const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
            const QString targetDll =
                flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
            const QString infocardXml =
                flatlas::infrastructure::XmlInfocard::wrapAsInfocard(request.infoCardText);
            QString idsError;
            int newGlobalId = 0;
            if (flatlas::infrastructure::IdsDataService::writeInfocardEntry(
                    dataset, targetDll, 0, infocardXml, &newGlobalId, &idsError)) {
                assignedIdsInfo = newGlobalId;
            } else {
                QMessageBox::warning(this, tr("Surprise erstellen"),
                                     tr("Der ids_info-Text konnte nicht in die IDS-Daten geschrieben werden.\n%1")
                                         .arg(idsError.isEmpty() ? tr("Unbekannter Fehler.") : idsError));
                return;
            }
        }

        auto obj = std::make_shared<SolarObject>();
        obj->setNickname(request.nickname);
        obj->setType(SolarObject::Wreck);
        obj->setArchetype(request.archetype);
        obj->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        obj->setRotation(QVector3D());
        if (!request.loadout.isEmpty())
            obj->setLoadout(request.loadout);
        if (assignedIdsName > 0)
            obj->setIdsName(assignedIdsName);
        if (assignedIdsInfo > 0)
            obj->setIdsInfo(assignedIdsInfo);
        obj->setComment(request.comment);

        QVector<QPair<QString, QString>> entries{
            {QStringLiteral("nickname"), request.nickname},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
            {QStringLiteral("archetype"), request.archetype},
        };
        if (assignedIdsName > 0)
            entries.append({QStringLiteral("ids_name"), QString::number(assignedIdsName)});
        if (assignedIdsInfo > 0)
            entries.append({QStringLiteral("ids_info"), QString::number(assignedIdsInfo)});
        if (!request.loadout.isEmpty())
            entries.append({QStringLiteral("loadout"), request.loadout});
        entries.append({QStringLiteral("visit"), QStringLiteral("0")});
        if (!request.comment.isEmpty())
            entries.append({QStringLiteral("comment"), request.comment});
        obj->setRawEntries(entries);

        auto *cmd = new AddObjectCommand(m_document.get(), obj, tr("Create Surprise"));
        flatlas::core::UndoManager::instance().push(cmd);
        refreshObjectList();
        m_selectedNicknames = {obj->nickname()};
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateIniEditorForSelection();
    };

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [placementGuard, finalizePlacement](const QPointF &scenePos) {
        finalizePlacement(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() { placementGuard->deleteLater(); });
    m_mapView->setPlacementMode(true,
                                tr("Klicke auf die Map, um '%1' zu platzieren. [Esc] oder Rechtsklick bricht ab.")
                                    .arg(request.nickname));
}

void SystemEditorPage::onCreatePatrolZone()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    QStringList existingZoneNicknames;
    for (const auto &zone : m_document->zones()) {
        if (zone)
            existingZoneNicknames.append(zone->nickname());
    }

    const QString systemToken = QFileInfo(m_document->filePath()).completeBaseName().toUpper();
    const QString suggested =
        suggestIndexedNickname(QStringLiteral("Zone_%1_path_patrol_")
                                   .arg(systemToken.isEmpty() ? QStringLiteral("SYSTEM") : systemToken),
                               existingZoneNicknames);
    CreatePatrolZoneDialog dialog(suggested,
                                  collectEncounterNicknames(m_document.get(), gameRoot),
                                  loadFactionDisplays(gameRoot),
                                  this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreatePatrolZoneRequest request = dialog.result();
    if (request.nickname.isEmpty() || request.encounter.isEmpty() || request.factionDisplay.isEmpty())
        return;

    beginPatrolZonePlacement(request);
}

void SystemEditorPage::onCreateJumpConnection()
{
    if (!m_document || !m_mapView || m_mapView->isPlacementModeActive())
        return;

    if (!ensureSavedForCrossSystemOperation(tr("Jump-Verbindung erstellen"),
                                            tr("Jump-Verbindungen bearbeiten immer beide Systemdateien direkt."))) {
        return;
    }

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const auto universe = loadUniverseForEditor(gameRoot);
    if (!universe || universe->systems.isEmpty()) {
        QMessageBox::warning(this, tr("Jump-Verbindung erstellen"),
                             tr("Es konnten keine gueltigen Systeme aus universe.ini geladen werden."));
        return;
    }
    const auto systems = universe->systems;

    flatlas::domain::SystemInfo sourceSystem;
    sourceSystem.nickname = m_document->name().trimmed();
    sourceSystem.displayName = m_document->displayName().trimmed();
    sourceSystem.filePath = m_document->filePath();

    JumpConnectionDialog dialog(this);
    dialog.setSourceSystem(sourceSystem, m_document.get());
    dialog.setSystems(systems);
    dialog.setUniverseConnections(universe->connections);
    dialog.setJumpHoleArchetypes(loadSolarArchetypesMatching(gameRoot, {QStringLiteral("jumphole"),
                                                                       QStringLiteral("jump_hole")}));
    dialog.setGateLoadouts(loadLoadoutsMatching(gameRoot, {QStringLiteral("jumpgate")}));
    dialog.setFactions(loadFactionDisplays(gameRoot));
    dialog.setPilots(loadPilotNicknames(gameRoot));
    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto connection = dialog.connection();
    JumpConnectionCreateRequest request;
    request.sourceSystemNickname = sourceSystem.nickname;
    request.sourceSystemDisplayName = sourceSystem.displayName.trimmed().isEmpty() ? sourceSystem.nickname : sourceSystem.displayName;
    request.sourceSystemFilePath = sourceSystem.filePath;
    request.destinationSystemNickname = connection.toSystem.trimmed();
    request.sourceObjectNickname = connection.fromObject.trimmed();
    request.destinationObjectNickname = connection.toObject.trimmed();
    request.kind = dialog.isJumpGate() ? QStringLiteral("gate") : QStringLiteral("hole");
    request.archetype = dialog.selectedArchetype();
    request.sourcePosition = dialog.sourcePosition();
    request.destinationPosition = dialog.destinationPosition();
    request.loadout = dialog.selectedLoadout();
    request.reputation = factionNicknameFromDisplay(dialog.selectedReputation());
    request.pilot = dialog.selectedPilot();
    request.behavior = dialog.behavior();
    request.difficultyLevel = dialog.difficultyLevel();

    for (const auto &system : systems) {
        if (system.nickname.compare(request.destinationSystemNickname, Qt::CaseInsensitive) != 0)
            continue;
        request.destinationSystemDisplayName =
            system.displayName.trimmed().isEmpty() ? system.nickname : system.displayName;
        request.destinationSystemFilePath = system.filePath;
        break;
    }

    if (request.destinationSystemFilePath.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Jump-Verbindung erstellen"),
                             tr("Das Zielsystem konnte nicht aufgeloest werden."));
        return;
    }

    QString errorMessage;
    if (!JumpConnectionService::createConnection(request, &errorMessage)) {
        QMessageBox::warning(this, tr("Jump-Verbindung erstellen"),
                             errorMessage.isEmpty() ? tr("Die Jump-Verbindung konnte nicht erstellt werden.")
                                                    : errorMessage);
        return;
    }

    loadFile(m_document->filePath());
}

void SystemEditorPage::onCreatePlanet()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    QStringList existingObjectNicknames;
    for (const auto &obj : m_document->objects()) {
        if (obj)
            existingObjectNicknames.append(obj->nickname());
    }

    const QString systemToken = QFileInfo(m_document->filePath()).completeBaseName().toUpper();
    const QString suggested =
        suggestIndexedNickname(QStringLiteral("%1_planet_").arg(systemToken.isEmpty() ? QStringLiteral("SYSTEM") : systemToken),
                               existingObjectNicknames);

    const PlanetCreationCatalog catalog = PlanetCreationService::loadCatalog(m_document.get(), gameRoot);
    CreatePlanetDialog dialog(suggested, catalog, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreatePlanetRequest request = dialog.result();
    if (request.nickname.isEmpty() || request.archetype.isEmpty() || request.ingameName.isEmpty())
        return;

    auto finalizePlacement = [this, request, gameRoot](const QPointF &scenePos) {
        if (!m_document)
            return;

        const QString deathZoneNickname = QStringLiteral("Zone_%1_death").arg(request.nickname);
        if (findObjectByNickname(request.nickname) || findZoneByNickname(deathZoneNickname)) {
            QMessageBox::warning(this, tr("Planet erstellen"),
                                 tr("Im aktuellen System existiert bereits ein Eintrag mit diesem Nickname."));
            return;
        }

        const QPointF worldXZ = MapScene::qtToFl(scenePos.x(), scenePos.y());
        const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
        const QString targetDll = flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);

        int assignedIdsName = 0;
        QString idsError;
        if (!flatlas::infrastructure::IdsDataService::writeStringEntry(
                dataset, targetDll, 0, request.ingameName, &assignedIdsName, &idsError)) {
            QMessageBox::warning(this, tr("Planet erstellen"),
                                 tr("Der Planetenname konnte nicht in die IDS-Daten geschrieben werden.\n%1")
                                     .arg(idsError.isEmpty() ? tr("Unbekannter Fehler.") : idsError));
            return;
        }

        const QString infocardXml = PlanetCreationService::wrapInfocardXml(request.infoCardText);
        int assignedIdsInfo = 0;
        if (!flatlas::infrastructure::IdsDataService::writeInfocardEntry(
                dataset, targetDll, 0, infocardXml, &assignedIdsInfo, &idsError)) {
            QMessageBox::warning(this, tr("Planet erstellen"),
                                 tr("Der Infocard-Text konnte nicht als neuer ids_info-Eintrag gespeichert werden.\n%1")
                                     .arg(idsError.isEmpty() ? tr("Unbekannter Fehler.") : idsError));
            return;
        }

        auto planet = std::make_shared<SolarObject>();
        planet->setNickname(request.nickname);
        planet->setType(SolarObject::Planet);
        planet->setArchetype(request.archetype);
        planet->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        planet->setRotation(QVector3D());
        planet->setIdsName(assignedIdsName);
        planet->setIdsInfo(assignedIdsInfo);
        planet->setRawEntries({
            {QStringLiteral("nickname"), request.nickname},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
            {QStringLiteral("archetype"), request.archetype},
            {QStringLiteral("spin"), QStringLiteral("0, 0, 0")},
            {QStringLiteral("atmosphere_range"), QString::number(request.atmosphereRange)},
            {QStringLiteral("ids_name"), QString::number(assignedIdsName)},
            {QStringLiteral("ids_info"), QString::number(assignedIdsInfo)},
        });

        auto deathZone = std::make_shared<ZoneItem>();
        deathZone->setNickname(deathZoneNickname);
        deathZone->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        deathZone->setRotation(QVector3D());
        deathZone->setShape(ZoneItem::Sphere);
        deathZone->setSize(QVector3D(static_cast<float>(request.deathZoneRadius), 0.0f, 0.0f));
        deathZone->setDamage(request.deathZoneDamage);
        deathZone->setSortKey(99);
        deathZone->setRawEntries({
            {QStringLiteral("nickname"), deathZoneNickname},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
            {QStringLiteral("shape"), QStringLiteral("SPHERE")},
            {QStringLiteral("size"), QString::number(request.deathZoneRadius)},
            {QStringLiteral("damage"), QString::number(request.deathZoneDamage)},
            {QStringLiteral("sort"), QStringLiteral("99.5")},
            {QStringLiteral("density"), QStringLiteral("0")},
            {QStringLiteral("relief_time"), QStringLiteral("0")},
            {QStringLiteral("ids_info"), QStringLiteral("0")},
        });

        auto *stack = flatlas::core::UndoManager::instance().stack();
        stack->beginMacro(tr("Planet erstellen"));
        stack->push(new AddObjectCommand(m_document.get(), planet, tr("Create Planet")));
        stack->push(new AddZoneCommand(m_document.get(), deathZone, tr("Create Planet Death Zone")));
        stack->endMacro();
        refreshObjectList();
        m_selectedNicknames = {planet->nickname()};
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateIniEditorForSelection();
    };

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [placementGuard, finalizePlacement](const QPointF &scenePos) {
        finalizePlacement(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() { placementGuard->deleteLater(); });
    m_mapView->setPlacementMode(true,
                                tr("Klicke auf die Map, um '%1' zu platzieren. [Esc] oder Rechtsklick bricht ab.")
                                    .arg(request.nickname));
}

void SystemEditorPage::onCreateBuoy()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    CreateBuoyDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreateBuoyRequest request = dialog.result();
    const int minimumCount = request.mode == CreateBuoyRequest::Mode::Line ? 2 : 3;
    if (request.count < minimumCount) {
        QMessageBox::warning(this, tr("Bojen erstellen"),
                             request.mode == CreateBuoyRequest::Mode::Line
                                 ? tr("Es muessen mindestens zwei Bojen erstellt werden.")
                                 : tr("Es muessen mindestens drei Bojen fuer einen Kreis erstellt werden."));
        return;
    }
    if (request.mode == CreateBuoyRequest::Mode::Line
        && request.lineConstraint == CreateBuoyRequest::LineConstraint::FixedSpacing
        && request.spacingMeters < 100) {
        QMessageBox::warning(this, tr("Bojen erstellen"),
                             tr("Der Abstand zwischen Bojen ist ungueltig."));
        return;
    }

    beginBuoyPlacement(request);
}

void SystemEditorPage::onCreateWeaponPlatform()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    QStringList existingObjectNicknames;
    for (const auto &obj : m_document->objects()) {
        if (obj)
            existingObjectNicknames.append(obj->nickname());
    }
    const QString systemToken = QFileInfo(m_document->filePath()).completeBaseName().toUpper();
    const QString suggested =
        suggestIndexedNickname(QStringLiteral("%1_Weapon_Platform_")
                                   .arg(systemToken.isEmpty() ? QStringLiteral("SYSTEM") : systemToken),
                               existingObjectNicknames);
    CreateWeaponPlatformDialog dialog(suggested,
                                      loadSolarArchetypesMatching(gameRoot, {QStringLiteral("weapon"),
                                                                             QStringLiteral("platform"),
                                                                             QStringLiteral("wplatform")}),
                                      loadLoadoutsMatching(gameRoot, {QStringLiteral("weapon"),
                                                                      QStringLiteral("platform")}),
                                      loadFactionDisplays(gameRoot),
                                      this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreateWeaponPlatformRequest request = dialog.result();
    if (request.nickname.isEmpty() || request.archetype.isEmpty())
        return;

    auto finalizePlacement = [this, request](const QPointF &scenePos) {
        if (!m_document)
            return;
        if (findObjectByNickname(request.nickname) || findZoneByNickname(request.nickname)) {
            QMessageBox::warning(this, tr("Waffenplattform erstellen"),
                                 tr("Im aktuellen System existiert bereits ein Eintrag mit diesem Nickname."));
            return;
        }

        const QPointF worldXZ = MapScene::qtToFl(scenePos.x(), scenePos.y());
        int assignedIdsName = 261164;
        if (!request.ingameName.isEmpty()) {
            const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
            const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
            const QString targetDll =
                flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
            QString idsError;
            int newGlobalId = 0;
            if (flatlas::infrastructure::IdsDataService::writeStringEntry(
                    dataset, targetDll, 261164, request.ingameName, &newGlobalId, &idsError)) {
                assignedIdsName = newGlobalId;
            } else {
                QMessageBox::warning(this, tr("Waffenplattform erstellen"),
                                     tr("Der Ingame Name konnte nicht in die IDS-Daten geschrieben werden.\n%1")
                                         .arg(idsError.isEmpty() ? tr("Unbekannter Fehler.") : idsError));
                return;
            }
        }

        auto obj = std::make_shared<SolarObject>();
        obj->setNickname(request.nickname);
        obj->setType(SolarObject::Weapons_Platform);
        obj->setArchetype(request.archetype);
        obj->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        obj->setRotation(QVector3D());
        obj->setIdsName(assignedIdsName);
        obj->setIdsInfo(66171);
        obj->setLoadout(request.loadout);
        QVector<QPair<QString, QString>> entries{
            {QStringLiteral("nickname"), request.nickname},
            {QStringLiteral("ids_name"), QString::number(assignedIdsName)},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
            {QStringLiteral("archetype"), request.archetype},
            {QStringLiteral("reputation"), factionNicknameFromDisplay(request.reputation)},
            {QStringLiteral("behavior"), request.behavior.isEmpty() ? QStringLiteral("NOTHING") : request.behavior},
            {QStringLiteral("ids_info"), QStringLiteral("66171")},
            {QStringLiteral("difficulty_level"), QString::number(request.difficultyLevel)},
        };
        if (!request.loadout.isEmpty())
            entries.append({QStringLiteral("loadout"), request.loadout});
        if (!request.pilot.isEmpty())
            entries.append({QStringLiteral("pilot"), request.pilot});
        entries.append({QStringLiteral("visit"), QStringLiteral("0")});
        obj->setRawEntries(entries);

        auto *cmd = new AddObjectCommand(m_document.get(), obj, tr("Create Weapon Platform"));
        flatlas::core::UndoManager::instance().push(cmd);
        refreshObjectList();
        m_selectedNicknames = {obj->nickname()};
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateIniEditorForSelection();
    };

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [placementGuard, finalizePlacement](const QPointF &scenePos) {
        finalizePlacement(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() { placementGuard->deleteLater(); });
    m_mapView->setPlacementMode(true,
                                tr("Klicke auf die Map, um '%1' zu platzieren. [Esc] oder Rechtsklick bricht ab.")
                                    .arg(request.nickname));
}

void SystemEditorPage::onCreateDepot()
{
    createQuickObject(SolarObject::Depot, QStringLiteral("new_depot"), QStringLiteral("depot"));
}

void SystemEditorPage::onCreateTradeLane()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    beginTradeLanePlacement();
}

void SystemEditorPage::onCreateRing()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    if (SolarObject *selectedHost = findRingHostForSelection()) {
        openRingDialogForHost(selectedHost, true);
        return;
    }

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard](const QPointF &scenePos) {
        SolarObject *hostObject = findRingHostAtScenePos(scenePos);
        placementGuard->deleteLater();
        if (!hostObject) {
            QMessageBox::information(this,
                                     tr("Ring erstellen"),
                                     tr("Bitte klicke auf einen Planeten oder eine Sonne, um einen Ring anzuhaengen."));
            onCreateRing();
            return;
        }
        openRingDialogForHost(hostObject, true);
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() { placementGuard->deleteLater(); });
    m_mapView->setPlacementMode(true,
                                tr("Klicke auf einen Planeten oder eine Sonne, um den Ring-Host auszuwaehlen."));
}

void SystemEditorPage::onEditRing()
{
    if (!m_document)
        return;

    SolarObject *hostObject = findRingHostForSelection();
    if (!hostObject || !RingEditService::hasRing(*hostObject)) {
        QMessageBox::information(this,
                                 tr("Ring bearbeiten"),
                                 tr("Bitte waehle ein Objekt mit vorhandenem Ring oder direkt die Ring-Zone aus."));
        return;
    }

    openRingDialogForHost(hostObject, false);
}

void SystemEditorPage::onEditTradeLane()
{
    if (!m_document)
        return;

    const TradeLaneChainDetection detection = TradeLaneEditService::inspectChain(*m_document, primarySelectedNickname());
    if (!detection.chain.has_value()) {
        QMessageBox::warning(this,
                             tr("Trade Lane bearbeiten"),
                             detection.errorMessage.trimmed().isEmpty()
                                 ? tr("Die ausgewaehlte Trade Lane konnte nicht erkannt werden.")
                                 : detection.errorMessage);
        return;
    }

    if (detection.issue == TradeLaneChainIssue::MissingPrevRing
        || detection.issue == TradeLaneChainIssue::MissingNextRing) {
        const QString brokenKey = detection.issue == TradeLaneChainIssue::MissingPrevRing
            ? QStringLiteral("prev_ring")
            : QStringLiteral("next_ring");
        const int answer = QMessageBox::question(
            this,
            tr("Trade Lane reparieren"),
            tr("Die Trade Lane referenziert einen fehlenden Ring '%1'.\n\n"
               "Soll der defekte %2-Verweis automatisch entfernt werden, damit die vorhandene Kette bearbeitet werden kann?")
                .arg(detection.referencedNickname, brokenKey),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (answer != QMessageBox::Yes)
            return;

        auto repairedRing = cloneSolarObject(detection.boundaryRing);
        if (!repairedRing) {
            QMessageBox::warning(this,
                                 tr("Trade Lane reparieren"),
                                 tr("Der defekte Ring konnte nicht fuer die Reparatur vorbereitet werden."));
            return;
        }
        repairedRing->setRawEntries(removeRawEntriesByKey(repairedRing->rawEntries(), brokenKey));

        auto *stack = flatlas::core::UndoManager::instance().stack();
        stack->beginMacro(tr("Trade Lane reparieren"));
        stack->push(new RemoveObjectCommand(m_document.get(), detection.boundaryRing, tr("Remove Broken Trade Lane Ring")));
        stack->push(new AddObjectCommand(m_document.get(), repairedRing, tr("Add Repaired Trade Lane Ring")));
        stack->endMacro();

        refreshObjectList();
        m_selectedNicknames = {repairedRing->nickname()};
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateSelectionSummary();
        onEditTradeLane();
        return;
    }

    const TradeLaneChain chain = *detection.chain;
    m_selectedNicknames = TradeLaneEditService::memberNicknames(chain);
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const auto &dialogCatalog = tradeLaneDialogCatalog(gameRoot);
    EditTradeLaneRequest initial;
    initial.startX = chain.startPosition.x();
    initial.startZ = chain.startPosition.z();
    initial.endX = chain.endPosition.x();
    initial.endZ = chain.endPosition.z();
    initial.ringCount = chain.rings.size();
    initial.archetype = chain.archetype;
    initial.loadout = chain.loadout;
    initial.reputationDisplay = chain.reputation;
    initial.difficultyLevel = chain.difficultyLevel;
    initial.pilot = chain.pilot;
    initial.routeName = idsDisplayTextFromTable(dialogCatalog.ids, chain.routeIdsName);
    initial.startSpaceName = idsDisplayTextFromTable(dialogCatalog.ids, chain.startSpaceNameId);
    initial.endSpaceName = idsDisplayTextFromTable(dialogCatalog.ids, chain.endSpaceNameId);

    const QString reputationDisplay = initial.reputationDisplay.isEmpty()
        ? QString()
        : dialogCatalog.factionDisplays.contains(initial.reputationDisplay)
            ? initial.reputationDisplay
            : QStringLiteral("%1 - %2").arg(initial.reputationDisplay, initial.reputationDisplay);
    initial.reputationDisplay = reputationDisplay;

    const QString laneSummary = tr("Lane: %1 -> %2\nRinge: %3")
        .arg(chain.rings.first()->nickname(), chain.rings.last()->nickname())
        .arg(chain.rings.size());
    EditTradeLaneDialog dialog(laneSummary,
                               dialogCatalog.archetypes,
                               dialogCatalog.loadouts,
                               dialogCatalog.factions,
                               dialogCatalog.pilots,
                               initial,
                               this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (dialog.deleteRequested()) {
        const auto answer = QMessageBox::question(
            this,
            tr("Trade Lane loeschen"),
            tr("Soll die komplette Trade Lane mit %1 Ringen wirklich geloescht werden?")
                .arg(chain.rings.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;

        auto *stack = flatlas::core::UndoManager::instance().stack();
        stack->beginMacro(tr("Trade Lane loeschen"));
        for (const auto &ring : chain.rings)
            stack->push(new RemoveObjectCommand(m_document.get(), ring, tr("Remove Trade Lane Ring")));
        stack->endMacro();

        refreshObjectList();
        m_selectedNicknames.clear();
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateSelectionSummary();
        updateIniEditorForSelection();
        return;
    }

    const EditTradeLaneRequest edited = dialog.result();
    TradeLaneEditRequest request;
    request.systemNickname = chain.systemNickname.trimmed().isEmpty()
        ? QFileInfo(m_document->filePath()).completeBaseName()
        : chain.systemNickname;
    request.startNumber = chain.startNumber;
    request.startPosition = QVector3D(static_cast<float>(edited.startX), 0.0f, static_cast<float>(edited.startZ));
    request.endPosition = QVector3D(static_cast<float>(edited.endX), 0.0f, static_cast<float>(edited.endZ));
    request.ringCount = edited.ringCount;
    request.archetype = edited.archetype.trimmed();
    request.loadout = edited.loadout.trimmed();
    request.reputation = factionNicknameFromDisplay(edited.reputationDisplay);
    request.behavior = chain.behavior;
    request.pilot = edited.pilot.trimmed();
    request.difficultyLevel = edited.difficultyLevel;
    request.idsInfo = chain.idsInfo;

    QString idsError;
    if (!resolveIdsStringInput(gameRoot,
                               edited.routeName,
                               chain.routeIdsName,
                               initial.routeName,
                               &request.routeIdsName,
                               &idsError)
        || !resolveIdsStringInput(gameRoot,
                                  edited.startSpaceName,
                                  chain.startSpaceNameId,
                                  initial.startSpaceName,
                                  &request.startSpaceNameId,
                                  &idsError)
        || !resolveIdsStringInput(gameRoot,
                                  edited.endSpaceName,
                                  chain.endSpaceNameId,
                                  initial.endSpaceName,
                                  &request.endSpaceNameId,
                                  &idsError)) {
        QMessageBox::warning(this,
                             tr("Trade Lane bearbeiten"),
                             idsError.trimmed().isEmpty()
                                 ? tr("Die IDS-Texte der Trade Lane konnten nicht aktualisiert werden.")
                                 : idsError);
        return;
    }

    QSet<QString> occupiedNicknames;
    QSet<QString> laneMembers;
    for (const QString &nickname : m_selectedNicknames)
        laneMembers.insert(nickname.trimmed().toLower());
    for (const auto &obj : m_document->objects()) {
        if (!obj)
            continue;
        const QString key = obj->nickname().trimmed().toLower();
        if (!laneMembers.contains(key))
            occupiedNicknames.insert(key);
    }

    QVector<std::shared_ptr<SolarObject>> replacementObjects;
    QString rebuildError;
    if (!TradeLaneEditService::buildReplacementObjects(chain,
                                                       request,
                                                       occupiedNicknames,
                                                       &replacementObjects,
                                                       &rebuildError)) {
        QMessageBox::warning(this,
                             tr("Trade Lane bearbeiten"),
                             rebuildError.trimmed().isEmpty()
                                 ? tr("Die Trade Lane konnte nicht neu aufgebaut werden.")
                                 : rebuildError);
        return;
    }

    auto *stack = flatlas::core::UndoManager::instance().stack();
    stack->beginMacro(tr("Trade Lane bearbeiten"));
    for (const auto &ring : chain.rings)
        stack->push(new RemoveObjectCommand(m_document.get(), ring, tr("Remove Trade Lane Ring")));
    for (const auto &ring : replacementObjects)
        stack->push(new AddObjectCommand(m_document.get(), ring, tr("Add Trade Lane Ring")));
    stack->endMacro();

    refreshObjectList();
    m_selectedNicknames.clear();
    for (const auto &ring : replacementObjects)
        m_selectedNicknames.append(ring->nickname());
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
}

void SystemEditorPage::onCreateBase()
{
    if (!m_document || !m_mapView || !m_mapScene || m_mapView->isPlacementModeActive())
        return;

    QString errorMessage;
    BaseEditState initialState = BaseEditService::makeCreateState(*m_document,
                                                                  flatlas::core::EditingContext::instance().primaryGamePath(),
                                                                  &errorMessage);
    if (!errorMessage.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Base erstellen"), errorMessage);
        return;
    }

    QHash<QString, QString> overrides;
    for (auto it = m_pendingTextFileWrites.constBegin(); it != m_pendingTextFileWrites.constEnd(); ++it)
        overrides.insert(it.key(), it.value().content);

    BaseEditDialog dialog(initialState, overrides, this);
    connect(&dialog, &BaseEditDialog::roomActivationRequested, this, [this](const QString &roomName, const QString &modelPath) {
        auto *mainWindow = qobject_cast<MainWindow *>(window());
        if (!mainWindow || !mainWindow->showModelInViewer(modelPath, tr("Room Preview: %1").arg(roomName))) {
            QMessageBox::warning(this,
                                 tr("Room Preview"),
                                 tr("Der ausgewaehlte Room konnte nicht im Haupt-3D-Viewer angezeigt werden."));
        }
    });
    if (dialog.exec() != QDialog::Accepted)
        return;

    const BaseEditState requestState = dialog.state();
    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard, requestState](const QPointF &scenePos) {
        placementGuard->deleteLater();
        QString errorMessage;
        BaseApplyResult applyResult;
        QHash<QString, QString> overrides;
        for (auto it = m_pendingTextFileWrites.constBegin(); it != m_pendingTextFileWrites.constEnd(); ++it)
            overrides.insert(it.key(), it.value().content);
        if (!BaseEditService::applyCreate(requestState,
                                          scenePos,
                                          flatlas::core::EditingContext::instance().primaryGamePath(),
                                          overrides,
                                          &applyResult,
                                          &errorMessage)) {
            QMessageBox::warning(this,
                                 tr("Base erstellen"),
                                 errorMessage.trimmed().isEmpty()
                                     ? tr("Die Base konnte nicht erstellt werden.")
                                     : errorMessage);
            return;
        }

        for (const BaseStagedWrite &write : applyResult.stagedWrites)
            stagePendingTextWrite(write.absolutePath, write.content);

        auto *cmd = new AddObjectCommand(m_document.get(), applyResult.createdObject, tr("Create Base"));
        flatlas::core::UndoManager::instance().push(cmd);
        m_selectedNicknames = {applyResult.createdObject->nickname()};
        refreshObjectList();
        syncTreeSelectionFromNicknames(m_selectedNicknames);
        syncSceneSelectionFromNicknames(m_selectedNicknames);
        updateSelectionSummary();
        updateIniEditorForSelection();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [placementGuard]() { placementGuard->deleteLater(); });
    m_mapView->setPlacementMode(true,
                                tr("Klicke auf die Map, um die neue Base zu platzieren."));
}

void SystemEditorPage::onCreateDockingRing()
{
    createQuickObject(SolarObject::DockingRing, QStringLiteral("new_docking_ring"), QStringLiteral("docking_ring"));
}

void SystemEditorPage::onCreateAsteroidNebulaZone()
{
    if (!m_document || !m_mapView || !m_mapScene)
        return;

    CreateFieldZoneDialog dialog(m_document.get(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    beginFieldZonePlacement(dialog.result());
}

bool SystemEditorPage::isFieldZone(const ZoneItem &zone) const
{
    QString sectionName;
    QString linkedZoneNickname;
    QString relativeFilePath;
    return resolveLinkedFieldSectionForZone(zone.nickname(),
                                            &sectionName,
                                            &linkedZoneNickname,
                                            &relativeFilePath);
}

bool SystemEditorPage::resolveLinkedFieldSectionForZone(const QString &zoneNickname,
                                                        QString *outSectionName,
                                                        QString *outFieldZoneNickname,
                                                        QString *outRelativeFilePath,
                                                        QString *outAbsoluteFilePath) const
{
    if (!m_document)
        return false;

    const IniDocument extras = SystemPersistence::extraSections(m_document.get());
    const QString target = zoneNickname.trimmed().toLower();
    for (const IniSection &section : extras) {
        const bool isFieldSection =
            section.name.compare(QStringLiteral("Nebula"), Qt::CaseInsensitive) == 0
            || section.name.compare(QStringLiteral("Asteroids"), Qt::CaseInsensitive) == 0;
        if (!isFieldSection)
            continue;

        const QString linkedZone = section.value(QStringLiteral("zone")).trimmed();
        const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
        if (linkedZone.compare(target, Qt::CaseInsensitive) != 0 || relativeFile.isEmpty())
            continue;

        if (outSectionName)
            *outSectionName = section.name;
        if (outFieldZoneNickname)
            *outFieldZoneNickname = linkedZone;
        if (outRelativeFilePath)
            *outRelativeFilePath = relativeFile;
        if (outAbsoluteFilePath) {
            QString absolutePath;
            const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
            QString normalizedRelative = relativeFile;
            normalizedRelative.replace(QLatin1Char('\\'), QLatin1Char('/'));
            normalizedRelative = normalizedRelative.trimmed();
            const QStringList candidates = normalizedRelative.startsWith(QStringLiteral("DATA/"), Qt::CaseInsensitive)
                ? QStringList{normalizedRelative}
                : QStringList{normalizedRelative, QStringLiteral("DATA/%1").arg(normalizedRelative)};
            for (const QString &candidate : candidates) {
                absolutePath = flatlas::core::PathUtils::ciResolvePath(gameRoot, candidate);
                if (!absolutePath.isEmpty())
                    break;
            }
            *outAbsoluteFilePath = absolutePath;
        }
        return true;
    }

    return false;
}

QString SystemEditorPage::pendingFieldIniText(const QString &relativeFilePath, const QString &absoluteFilePath) const
{
    const QString relativeKey = relativeFilePath.trimmed().toLower();
    if (m_pendingGeneratedZoneFiles.contains(relativeKey))
        return m_pendingGeneratedZoneFiles.value(relativeKey).content;

    const QString absoluteKey = normalizedPathKey(absoluteFilePath);
    if (!absoluteKey.isEmpty() && m_pendingTextFileWrites.contains(absoluteKey))
        return m_pendingTextFileWrites.value(absoluteKey).content;

    QFile file(absoluteFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QByteArray raw = file.readAll();
    QString text = QString::fromUtf8(raw);
    if (text.isEmpty() && !raw.isEmpty())
        text = QString::fromLocal8Bit(raw);
    return text;
}

void SystemEditorPage::stageFieldIniText(const QString &relativeFilePath,
                                         const QString &absoluteFilePath,
                                         const QString &content)
{
    const QString relativeKey = relativeFilePath.trimmed().toLower();
    if (m_pendingGeneratedZoneFiles.contains(relativeKey)) {
        PendingGeneratedZoneFile updated = m_pendingGeneratedZoneFiles.value(relativeKey);
        updated.content = content;
        m_pendingGeneratedZoneFiles.insert(relativeKey, updated);
        refreshDocumentDirtyState();
        return;
    }

    const QString absoluteKey = normalizedPathKey(absoluteFilePath);
    if (!absoluteKey.isEmpty())
        m_pendingTextFileWrites.insert(absoluteKey, PendingTextFileWrite{absoluteFilePath, content});
    refreshDocumentDirtyState();
}

bool SystemEditorPage::resolveLinkedFieldInfoForExclusion(const QString &exclusionZoneNickname,
                                                          QString *outSectionName,
                                                          QString *outFieldZoneNickname,
                                                          QString *outRelativeFilePath,
                                                          QString *outAbsoluteFilePath,
                                                          QString *outCurrentText,
                                                          ExclusionShellSettings *outShellSettings) const
{
    if (!m_document)
        return false;

    const QString target = exclusionZoneNickname.trimmed();
    const IniDocument extras = SystemPersistence::extraSections(m_document.get());
    for (const IniSection &section : extras) {
        const bool isFieldSection =
            section.name.compare(QStringLiteral("Nebula"), Qt::CaseInsensitive) == 0
            || section.name.compare(QStringLiteral("Asteroids"), Qt::CaseInsensitive) == 0;
        if (!isFieldSection)
            continue;

        const QString linkedZone = section.value(QStringLiteral("zone")).trimmed();
        const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
        QString absoluteFile;
        if (relativeFile.isEmpty())
            continue;
        resolveLinkedFieldSectionForZone(linkedZone, nullptr, nullptr, nullptr, &absoluteFile);
        const QString currentText = pendingFieldIniText(relativeFile, absoluteFile);
        if (currentText.isEmpty())
            continue;
        bool found = false;
        ExclusionShellSettings settings = ExclusionZoneUtils::readFieldIniExclusionSettings(currentText, target, &found);
        if (!found)
            continue;

        if (outSectionName)
            *outSectionName = section.name;
        if (outFieldZoneNickname)
            *outFieldZoneNickname = linkedZone;
        if (outRelativeFilePath)
            *outRelativeFilePath = relativeFile;
        if (outAbsoluteFilePath)
            *outAbsoluteFilePath = absoluteFile;
        if (outCurrentText)
            *outCurrentText = currentText;
        if (outShellSettings)
            *outShellSettings = settings;
        return true;
    }

    return false;
}

void SystemEditorPage::onCreateExclusionZone()
{
    if (!m_document)
        return;

    const QString selectedNickname = primarySelectedNickname();
    ZoneItem *fieldZone = findZoneByNickname(selectedNickname);
    if (!fieldZone || !isFieldZone(*fieldZone)) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Bitte wähle zuerst eine Nebel- oder Asteroiden-Feldzone aus."));
        return;
    }

    QString sectionName;
    QString linkedZoneNickname;
    QString relativeFilePath;
    if (!resolveLinkedFieldSectionForZone(fieldZone->nickname(),
                                          &sectionName,
                                          &linkedZoneNickname,
                                          &relativeFilePath)) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Für die ausgewählte Feld-Zone konnte keine verknüpfte Referenzdatei ermittelt werden."));
        return;
    }

    QStringList existingZoneNicknames;
    for (const auto &zone : m_document->zones()) {
        if (zone)
            existingZoneNicknames.append(zone->nickname());
    }

    const QString systemNickname = QFileInfo(m_document->filePath()).completeBaseName();
    const QString suggestedNickname =
        ExclusionZoneUtils::generateExclusionNickname(systemNickname, fieldZone->nickname(), existingZoneNicknames);
    const bool supportsShell = sectionName.compare(QStringLiteral("Nebula"), Qt::CaseInsensitive) == 0;
    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    CreateExclusionZoneDialog dialog(suggestedNickname,
                                     fieldZone->nickname(),
                                     relativeFilePath,
                                     supportsShell,
                                     supportsShell ? ExclusionZoneUtils::nebulaShellOptionsForGamePath(gameRoot) : QStringList{},
                                     this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    beginExclusionZonePlacement(dialog.result());
}

void SystemEditorPage::beginFieldZonePlacement(const CreateFieldZoneResult &request)
{
    cancelFieldZonePlacement();
    m_pendingFieldZoneRequest = std::make_unique<CreateFieldZoneResult>(request);
    m_pendingFieldZoneHasCenter = false;
    m_pendingFieldZoneCenterScenePos = QPointF();

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard](const QPointF &scenePos) {
        if (!m_pendingFieldZoneRequest || !m_mapView)
            return;
        m_pendingFieldZoneHasCenter = true;
        m_pendingFieldZoneCenterScenePos = scenePos;
        m_mapView->viewport()->installEventFilter(this);
        m_mapView->viewport()->setMouseTracking(true);
        m_mapView->viewport()->setToolTip(tr("Maus bewegen und erneut klicken, um die Zonengröße festzulegen. Esc oder Rechtsklick bricht ab."));
        updateFieldZonePlacementPreview(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [this, placementGuard]() {
        cancelFieldZonePlacement();
        placementGuard->deleteLater();
    });

    m_mapView->setPlacementMode(true,
        tr("Klicke auf die Map, um '%1' zu platzieren.").arg(request.nickname));
}

void SystemEditorPage::beginSimpleZonePlacement(const CreateSimpleZoneRequest &request)
{
    cancelSimpleZonePlacement();
    m_pendingSimpleZoneRequest = std::make_unique<CreateSimpleZoneRequest>(request);
    m_pendingSimpleZoneHasCenter = false;
    m_pendingSimpleZoneCenterScenePos = QPointF();

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard](const QPointF &scenePos) {
        if (!m_pendingSimpleZoneRequest || !m_mapView)
            return;
        m_pendingSimpleZoneHasCenter = true;
        m_pendingSimpleZoneCenterScenePos = scenePos;
        m_mapView->viewport()->installEventFilter(this);
        m_mapView->viewport()->setMouseTracking(true);
        updateSimpleZonePlacementPreview(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [this, placementGuard]() {
        cancelSimpleZonePlacement();
        placementGuard->deleteLater();
    });

    m_mapView->setPlacementMode(true,
                                tr("Klicke auf die Map, um '%1' zu platzieren.").arg(request.nickname));
}

void SystemEditorPage::beginPatrolZonePlacement(const CreatePatrolZoneRequest &request)
{
    cancelPatrolZonePlacement();
    m_pendingPatrolZoneRequest = std::make_unique<CreatePatrolZoneRequest>(request);
    m_pendingPatrolZoneHasStart = false;
    m_pendingPatrolZoneStartScenePos = QPointF();
    m_pendingPatrolZoneHasEnd = false;
    m_pendingPatrolZoneEndScenePos = QPointF();
    m_pendingPatrolZoneHalfWidthScene = 0.0;
    m_pendingPatrolZoneStep = 1;

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard](const QPointF &scenePos) {
        if (!m_pendingPatrolZoneRequest || !m_mapView)
            return;
        m_pendingPatrolZoneHasStart = true;
        m_pendingPatrolZoneStartScenePos = scenePos;
        m_pendingPatrolZoneStep = 2;
        m_mapView->viewport()->installEventFilter(this);
        m_mapView->viewport()->setMouseTracking(true);
        updatePatrolZonePlacementPreview(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [this, placementGuard]() {
        cancelPatrolZonePlacement();
        placementGuard->deleteLater();
    });

    m_mapView->setPlacementMode(true,
                                tr("1. Klick Startpunkt fuer '%1'. 2. Klick Endpunkt. 3. Klick Breite. 4. Klick speichert.")
                                    .arg(request.nickname));
}

void SystemEditorPage::beginBuoyPlacement(const CreateBuoyRequest &request)
{
    cancelBuoyPlacement();
    m_pendingBuoyRequest = std::make_unique<CreateBuoyRequest>(request);
    m_pendingBuoyHasAnchor = false;
    m_pendingBuoyAnchorScenePos = QPointF();
    m_pendingBuoyStep = 1;

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard](const QPointF &scenePos) {
        if (!m_pendingBuoyRequest || !m_mapView)
            return;
        m_pendingBuoyHasAnchor = true;
        m_pendingBuoyAnchorScenePos = scenePos;
        m_pendingBuoyStep = 2;
        m_mapView->viewport()->installEventFilter(this);
        m_mapView->viewport()->setMouseTracking(true);
        updateBuoyPlacementPreview(scenePos);
        m_mapView->setPlacementMode(true,
                                    m_pendingBuoyRequest->mode == CreateBuoyRequest::Mode::Line
                                        ? tr("2. Klick bestimmt die Richtung fuer das Bojenmuster. [Esc] oder Rechtsklick bricht ab.")
                                        : tr("2. Klick bestimmt den Radius fuer das Bojenmuster. [Esc] oder Rechtsklick bricht ab."));
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [this, placementGuard]() {
        cancelBuoyPlacement();
        placementGuard->deleteLater();
    });

    m_mapView->setPlacementMode(true,
                                request.mode == CreateBuoyRequest::Mode::Line
                                    ? (request.lineConstraint == CreateBuoyRequest::LineConstraint::FixedCount
                                           ? tr("1. Klick setzt den Startpunkt. Linienmodus: feste Anzahl, Abstand wird waehrend der Platzierung berechnet.")
                                           : tr("1. Klick setzt den Startpunkt. Linienmodus: fester Abstand, Anzahl wird waehrend der Platzierung berechnet."))
                                    : tr("1. Klick setzt den Mittelpunkt des Bojenkreises."));
}

void SystemEditorPage::updateBuoyPlacementPreview(const QPointF &currentScenePos)
{
    if (!m_pendingBuoyRequest || !m_pendingBuoyHasAnchor || !m_mapScene)
        return;

    QVector<QPointF> previewPositions;
    const auto mode = m_pendingBuoyRequest->mode;
    const int count = std::max(m_pendingBuoyRequest->count,
                               mode == CreateBuoyRequest::Mode::Line ? 2 : 3);

    if (mode == CreateBuoyRequest::Mode::Line) {
        if (!m_buoyLinePreview) {
            const QColor stroke(120, 200, 255, 220);
            m_buoyLinePreview = new QGraphicsLineItem();
            m_buoyLinePreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
            m_buoyLinePreview->setZValue(9998.0);
            m_mapScene->addItem(m_buoyLinePreview);
        }

        const QLineF directionLine(m_pendingBuoyAnchorScenePos, currentScenePos);
        const qreal directionLength = directionLine.length();
        if (directionLength > 0.001) {
            int countForPreview = std::max(m_pendingBuoyRequest->count, 2);
            qreal spacingScene = std::max<qreal>(static_cast<qreal>(m_pendingBuoyRequest->spacingMeters) / 100.0, 1.0);
            double derivedSpacingMeters = 0.0;
            if (m_pendingBuoyRequest->lineConstraint == CreateBuoyRequest::LineConstraint::FixedCount) {
                derivedSpacingMeters = derivedBuoySpacingMetersForLine(directionLength, countForPreview);
                spacingScene = std::max<qreal>(directionLength / std::max(countForPreview - 1, 1), 1.0);
            } else {
                countForPreview = derivedBuoyCountForLine(directionLength, m_pendingBuoyRequest->spacingMeters);
            }
            const QPointF unit((currentScenePos.x() - m_pendingBuoyAnchorScenePos.x()) / directionLength,
                               (currentScenePos.y() - m_pendingBuoyAnchorScenePos.y()) / directionLength);
            const QPointF lineEnd = m_pendingBuoyAnchorScenePos
                                    + QPointF(unit.x() * spacingScene * (countForPreview - 1),
                                              unit.y() * spacingScene * (countForPreview - 1));
            m_buoyLinePreview->setLine(QLineF(m_pendingBuoyAnchorScenePos, lineEnd));
            previewPositions.reserve(countForPreview);
            for (int index = 0; index < countForPreview; ++index) {
                previewPositions.append(m_pendingBuoyAnchorScenePos
                                        + QPointF(unit.x() * spacingScene * index,
                                                  unit.y() * spacingScene * index));
            }
            const QString helpText =
                m_pendingBuoyRequest->lineConstraint == CreateBuoyRequest::LineConstraint::FixedCount
                    ? tr("2. Klick bestaetigt die Linie. Feste Anzahl: %1, berechneter Abstand: %2 m. [Esc] oder Rechtsklick bricht ab.")
                          .arg(countForPreview)
                          .arg(QString::number(derivedSpacingMeters, 'f', 0))
                    : tr("2. Klick bestaetigt die Linie. Fester Abstand: %1 m, berechnete Anzahl: %2. [Esc] oder Rechtsklick bricht ab.")
                          .arg(m_pendingBuoyRequest->spacingMeters)
                          .arg(countForPreview);
            m_mapView->setPlacementMode(true, helpText);
            emit selectionStatusChanged(
                m_pendingBuoyRequest->lineConstraint == CreateBuoyRequest::LineConstraint::FixedCount
                    ? tr("Bojenlinie: %1 Bojen, Abstand %2 m").arg(countForPreview).arg(QString::number(derivedSpacingMeters, 'f', 0))
                    : tr("Bojenlinie: Abstand %1 m, %2 Bojen").arg(m_pendingBuoyRequest->spacingMeters).arg(countForPreview));
        } else {
            m_buoyLinePreview->setLine(QLineF(m_pendingBuoyAnchorScenePos, currentScenePos));
        }
        if (m_buoyCirclePreview)
            m_buoyCirclePreview->setVisible(false);
    } else {
        if (!m_buoyCirclePreview) {
            const QColor stroke(120, 200, 255, 220);
            const QColor fill(120, 200, 255, 22);
            m_buoyCirclePreview = new QGraphicsEllipseItem();
            m_buoyCirclePreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
            m_buoyCirclePreview->setBrush(fill);
            m_buoyCirclePreview->setZValue(9998.0);
            m_mapScene->addItem(m_buoyCirclePreview);
        }

        const qreal radius = std::max<qreal>(QLineF(m_pendingBuoyAnchorScenePos, currentScenePos).length(), 1.0);
        m_buoyCirclePreview->setVisible(true);
        m_buoyCirclePreview->setRect(m_pendingBuoyAnchorScenePos.x() - radius,
                                     m_pendingBuoyAnchorScenePos.y() - radius,
                                     radius * 2.0,
                                     radius * 2.0);
        previewPositions.reserve(count);
        for (int index = 0; index < count; ++index) {
            const double angle = (2.0 * M_PI * static_cast<double>(index)) / static_cast<double>(count);
            previewPositions.append(QPointF(m_pendingBuoyAnchorScenePos.x() + std::cos(angle) * radius,
                                            m_pendingBuoyAnchorScenePos.y() + std::sin(angle) * radius));
        }
        if (m_buoyLinePreview)
            m_buoyLinePreview->setVisible(false);
    }

    while (m_buoyMarkerPreviews.size() < previewPositions.size()) {
        auto *marker = new QGraphicsEllipseItem();
        marker->setPen(QPen(QColor(120, 200, 255, 230), 1.5));
        marker->setBrush(QColor(120, 200, 255, 80));
        marker->setZValue(10000.0);
        m_mapScene->addItem(marker);
        m_buoyMarkerPreviews.append(marker);
    }

    constexpr qreal kMarkerRadius = 4.0;
    for (int index = 0; index < m_buoyMarkerPreviews.size(); ++index) {
        auto *marker = m_buoyMarkerPreviews[index];
        if (index < previewPositions.size()) {
            const QPointF pos = previewPositions[index];
            marker->setRect(pos.x() - kMarkerRadius,
                            pos.y() - kMarkerRadius,
                            kMarkerRadius * 2.0,
                            kMarkerRadius * 2.0);
            marker->setVisible(true);
        } else {
            marker->setVisible(false);
        }
    }
}

void SystemEditorPage::beginTradeLanePlacement()
{
    cancelTradeLanePlacement();
    m_pendingTradeLaneHasStart = false;
    m_pendingTradeLaneStartScenePos = QPointF();

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard](const QPointF &scenePos) {
        if (!m_mapView)
            return;
        m_pendingTradeLaneHasStart = true;
        m_pendingTradeLaneStartScenePos = scenePos;
        m_mapView->viewport()->installEventFilter(this);
        m_mapView->viewport()->setMouseTracking(true);
        updateTradeLanePlacementPreview(scenePos);
        m_mapView->setPlacementMode(
            true,
            tr("2. Klick setzt das Ende der Trade Lane. Die Ring-Kette wird danach konfiguriert. [Esc] oder Rechtsklick bricht ab."));
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [this, placementGuard]() {
        cancelTradeLanePlacement();
        placementGuard->deleteLater();
    });

    m_mapView->setPlacementMode(true, tr("1. Klick setzt den Startpunkt der Trade Lane."));
}

void SystemEditorPage::updateTradeLanePlacementPreview(const QPointF &currentScenePos)
{
    if (!m_pendingTradeLaneHasStart || !m_mapScene)
        return;

    if (!m_tradeLanePlacementPreview) {
        const QColor stroke(120, 190, 255, 230);
        m_tradeLanePlacementPreview = new QGraphicsLineItem();
        m_tradeLanePlacementPreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
        m_tradeLanePlacementPreview->setZValue(9998.0);
        m_mapScene->addItem(m_tradeLanePlacementPreview);
    }

    m_tradeLanePlacementPreview->setLine(QLineF(m_pendingTradeLaneStartScenePos, currentScenePos));
}

void SystemEditorPage::updateFieldZonePlacementPreview(const QPointF &currentScenePos)
{
    if (!m_pendingFieldZoneRequest || !m_pendingFieldZoneHasCenter || !m_mapScene)
        return;

    if (!m_fieldZonePlacementPreview) {
        const QColor stroke(93, 184, 255, 220);
        const QColor fill(93, 184, 255, 38);
        m_fieldZonePlacementPreview = new QGraphicsEllipseItem();
        m_fieldZonePlacementPreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
        m_fieldZonePlacementPreview->setBrush(fill);
        m_fieldZonePlacementPreview->setZValue(10000.0);
        m_mapScene->addItem(m_fieldZonePlacementPreview);
    }

    const QRectF rect(QPointF(m_pendingFieldZoneCenterScenePos.x() - std::abs(currentScenePos.x() - m_pendingFieldZoneCenterScenePos.x()),
                              m_pendingFieldZoneCenterScenePos.y() - std::abs(currentScenePos.y() - m_pendingFieldZoneCenterScenePos.y())),
                      QPointF(m_pendingFieldZoneCenterScenePos.x() + std::abs(currentScenePos.x() - m_pendingFieldZoneCenterScenePos.x()),
                              m_pendingFieldZoneCenterScenePos.y() + std::abs(currentScenePos.y() - m_pendingFieldZoneCenterScenePos.y())));
    m_fieldZonePlacementPreview->setRect(rect.normalized());
}

void SystemEditorPage::updateSimpleZonePlacementPreview(const QPointF &currentScenePos)
{
    if (!m_pendingSimpleZoneRequest || !m_pendingSimpleZoneHasCenter || !m_mapScene)
        return;

    if (!m_simpleZonePlacementPreview) {
        const QColor stroke(170, 220, 120, 220);
        const QColor fill(170, 220, 120, 30);
        m_simpleZonePlacementPreview = new QGraphicsEllipseItem();
        m_simpleZonePlacementPreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
        m_simpleZonePlacementPreview->setBrush(fill);
        m_simpleZonePlacementPreview->setZValue(9999.0);
        m_mapScene->addItem(m_simpleZonePlacementPreview);
    }

    const QRectF rect(QPointF(m_pendingSimpleZoneCenterScenePos.x() - std::abs(currentScenePos.x() - m_pendingSimpleZoneCenterScenePos.x()),
                              m_pendingSimpleZoneCenterScenePos.y() - std::abs(currentScenePos.y() - m_pendingSimpleZoneCenterScenePos.y())),
                      QPointF(m_pendingSimpleZoneCenterScenePos.x() + std::abs(currentScenePos.x() - m_pendingSimpleZoneCenterScenePos.x()),
                              m_pendingSimpleZoneCenterScenePos.y() + std::abs(currentScenePos.y() - m_pendingSimpleZoneCenterScenePos.y())));
    m_simpleZonePlacementPreview->setRect(rect.normalized());
}

void SystemEditorPage::updatePatrolZonePlacementPreview(const QPointF &currentScenePos)
{
    if (!m_pendingPatrolZoneRequest || !m_pendingPatrolZoneHasStart || !m_mapScene)
        return;

    if (m_pendingPatrolZoneStep == 2) {
        if (!m_patrolZonePlacementPreview) {
            const QColor stroke(220, 200, 120, 220);
            m_patrolZonePlacementPreview = new QGraphicsLineItem();
            m_patrolZonePlacementPreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
            m_patrolZonePlacementPreview->setZValue(9999.0);
            m_mapScene->addItem(m_patrolZonePlacementPreview);
        }
        m_patrolZonePlacementPreview->setLine(QLineF(m_pendingPatrolZoneStartScenePos, currentScenePos));
        return;
    }

    if (m_pendingPatrolZoneStep == 3 && m_pendingPatrolZoneHasEnd) {
        const qreal halfWidth = std::max<qreal>(1.0, pointLineDistance(currentScenePos,
                                                                        m_pendingPatrolZoneStartScenePos,
                                                                        m_pendingPatrolZoneEndScenePos));
        if (!m_patrolZoneWidthPreview) {
            const QColor stroke(220, 200, 120, 220);
            const QColor fill(220, 200, 120, 30);
            m_patrolZoneWidthPreview = new QGraphicsPolygonItem();
            m_patrolZoneWidthPreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
            m_patrolZoneWidthPreview->setBrush(fill);
            m_patrolZoneWidthPreview->setZValue(9999.0);
            m_mapScene->addItem(m_patrolZoneWidthPreview);
        }
        m_patrolZoneWidthPreview->setPolygon(
            orientedRectPolygon(m_pendingPatrolZoneStartScenePos, m_pendingPatrolZoneEndScenePos, halfWidth));
    }
}

void SystemEditorPage::finalizeFieldZonePlacement(const QPointF &edgeScenePos)
{
    if (!m_document || !m_pendingFieldZoneRequest)
        return;

    const QPointF centerScenePos = m_pendingFieldZoneCenterScenePos;
    const QPointF centerFl = MapScene::qtToFl(centerScenePos.x(), centerScenePos.y());
    const QPointF edgeFl = MapScene::qtToFl(edgeScenePos.x(), edgeScenePos.y());
    const double sizeX = std::max(std::abs(edgeFl.x() - centerFl.x()), 500.0);
    const double sizeZ = std::max(std::abs(edgeFl.y() - centerFl.y()), 500.0);
    const double sizeY = std::min(sizeX, sizeZ);

    const QString systemFileStem = QFileInfo(m_document->filePath()).completeBaseName();
    const QString generatedFileName =
        suggestedGeneratedFieldFileName(systemFileStem, m_pendingFieldZoneRequest->nickname, m_pendingFieldZoneRequest->type);
    const QString relativeDir =
        (m_pendingFieldZoneRequest->type == CreateFieldZoneResult::Type::Asteroid)
            ? QStringLiteral("solar\\ASTEROIDS")
            : QStringLiteral("solar\\NEBULA");
    const QString relativeFilePath = relativeDir + QLatin1Char('\\') + generatedFileName;

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString solarDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR"));
    const QString targetDirName =
        (m_pendingFieldZoneRequest->type == CreateFieldZoneResult::Type::Asteroid)
            ? QStringLiteral("ASTEROIDS")
            : QStringLiteral("NEBULA");
    QString targetDir = flatlas::core::PathUtils::ciResolvePath(solarDir, targetDirName);
    if (targetDir.isEmpty())
        targetDir = QDir(solarDir).absoluteFilePath(targetDirName);
    const QString absoluteGeneratedPath = QDir(targetDir).absoluteFilePath(generatedFileName);

    QFile sourceFile(m_pendingFieldZoneRequest->referenceAbsolutePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Zone erstellen"),
                             tr("Die Referenzdatei konnte nicht gelesen werden:\n%1")
                                 .arg(m_pendingFieldZoneRequest->referenceAbsolutePath));
        cancelFieldZonePlacement();
        return;
    }

    const QString sourceSubdir =
        (m_pendingFieldZoneRequest->type == CreateFieldZoneResult::Type::Asteroid)
            ? QStringLiteral("solar\\ASTEROIDS")
            : QStringLiteral("solar\\NEBULA");
    const QByteArray sourceRaw = sourceFile.readAll();
    QString sourceText = QString::fromUtf8(sourceRaw);
    if (sourceText.isEmpty() && !sourceRaw.isEmpty())
        sourceText = QString::fromLocal8Bit(sourceRaw);
    const QString generatedContent = normalizedGeneratedZoneTemplateText(
        sourceText,
        QStringLiteral("; Copied by FL Atlas from file: %1\\%2")
            .arg(sourceSubdir, m_pendingFieldZoneRequest->referenceFileName));

    int assignedIdsName = 0;
    if (!m_pendingFieldZoneRequest->ingameName.trimmed().isEmpty()) {
        const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
        const QString targetDll =
            flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
        QString idsError;
        int newGlobalId = 0;
        if (!flatlas::infrastructure::IdsDataService::writeStringEntry(
                dataset, targetDll, 0, m_pendingFieldZoneRequest->ingameName.trimmed(), &newGlobalId, &idsError)) {
            QMessageBox::warning(this, tr("Zone erstellen"),
                                 idsError.trimmed().isEmpty()
                                     ? tr("Der Ingame-Name konnte nicht als IDS-Eintrag geschrieben werden.")
                                     : tr("Der Ingame-Name konnte nicht als IDS-Eintrag geschrieben werden:\n%1")
                                           .arg(idsError));
            cancelFieldZonePlacement();
            return;
        }
        assignedIdsName = newGlobalId;
    }

    auto zone = std::make_shared<ZoneItem>();
    zone->setNickname(m_pendingFieldZoneRequest->nickname);
    zone->setPosition(QVector3D(static_cast<float>(centerFl.x()), 0.0f, static_cast<float>(centerFl.y())));
    zone->setRotation(QVector3D(0.0f, 0.0f, 0.0f));
    zone->setShape(ZoneItem::Ellipsoid);
    zone->setSize(QVector3D(static_cast<float>(sizeX), static_cast<float>(sizeY), static_cast<float>(sizeZ)));
    zone->setDamage(m_pendingFieldZoneRequest->damage);
    zone->setSortKey(m_pendingFieldZoneRequest->sort);
    zone->setInterference(m_pendingFieldZoneRequest->hasInterference
                              ? static_cast<float>(m_pendingFieldZoneRequest->interference)
                              : 0.0f);
    zone->setComment(m_pendingFieldZoneRequest->comment);

    QVector<QPair<QString, QString>> entries;
    entries.append({QStringLiteral("nickname"), m_pendingFieldZoneRequest->nickname});
    entries.append({QStringLiteral("ids_name"), QString::number(assignedIdsName)});
    entries.append({QStringLiteral("pos"),
                    QStringLiteral("%1, 0, %2")
                        .arg(centerFl.x(), 0, 'f', 2)
                        .arg(centerFl.y(), 0, 'f', 2)});
    entries.append({QStringLiteral("rotate"), QStringLiteral("0, 0, 0")});
    entries.append({QStringLiteral("shape"), QStringLiteral("ELLIPSOID")});
    entries.append({QStringLiteral("size"),
                    QStringLiteral("%1, %2, %3")
                        .arg(sizeX, 0, 'f', 0)
                        .arg(sizeY, 0, 'f', 0)
                        .arg(sizeZ, 0, 'f', 0)});
    entries.append({QStringLiteral("property_flags"), QString::number(m_pendingFieldZoneRequest->propertyFlags)});
    entries.append({QStringLiteral("ids_info"), QStringLiteral("66146")});
    entries.append({QStringLiteral("visit"), QString::number(m_pendingFieldZoneRequest->visit)});
    entries.append({QStringLiteral("damage"), QString::number(m_pendingFieldZoneRequest->damage)});
    if (m_pendingFieldZoneRequest->hasInterference)
        entries.append({QStringLiteral("interference"),
                        QString::number(m_pendingFieldZoneRequest->interference, 'f', 2)});
    if (!m_pendingFieldZoneRequest->music.trimmed().isEmpty())
        entries.append({QStringLiteral("Music"), m_pendingFieldZoneRequest->music.trimmed()});
    if (!m_pendingFieldZoneRequest->spacedust.trimmed().isEmpty()) {
        entries.append({QStringLiteral("spacedust"), m_pendingFieldZoneRequest->spacedust.trimmed()});
        entries.append({QStringLiteral("spacedust_maxparticles"),
                        QString::number(m_pendingFieldZoneRequest->spacedustMaxParticles)});
    }
    entries.append({QStringLiteral("sort"), QString::number(m_pendingFieldZoneRequest->sort)});
    zone->setRawEntries(entries);

    addLinkedFieldSection(
        m_pendingFieldZoneRequest->type == CreateFieldZoneResult::Type::Asteroid
            ? QStringLiteral("Asteroids")
            : QStringLiteral("Nebula"),
        zone->nickname(),
        relativeFilePath);
    m_pendingGeneratedZoneFiles.insert(relativeFilePath.toLower(),
                                       PendingGeneratedZoneFile{absoluteGeneratedPath, generatedContent});

    m_document->addZone(zone);
    refreshObjectList();
    m_selectedNicknames = {zone->nickname()};
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateIniEditorForSelection();
    cancelFieldZonePlacement();
}

void SystemEditorPage::beginExclusionZonePlacement(const CreateExclusionZoneResult &request)
{
    cancelExclusionZonePlacement();
    m_pendingExclusionZoneRequest = std::make_unique<CreateExclusionZoneResult>(request);
    m_pendingExclusionZoneHasCenter = false;
    m_pendingExclusionZoneCenterScenePos = QPointF();

    auto *placementGuard = new QObject(this);
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementClicked,
            placementGuard, [this, placementGuard](const QPointF &scenePos) {
        if (!m_pendingExclusionZoneRequest || !m_mapView)
            return;
        m_pendingExclusionZoneHasCenter = true;
        m_pendingExclusionZoneCenterScenePos = scenePos;
        m_mapView->viewport()->installEventFilter(this);
        m_mapView->viewport()->setMouseTracking(true);
        updateExclusionZonePlacementPreview(scenePos);
        placementGuard->deleteLater();
    });
    connect(m_mapView, &flatlas::rendering::SystemMapView::placementCanceled,
            placementGuard, [this, placementGuard]() {
        cancelExclusionZonePlacement();
        placementGuard->deleteLater();
    });

    m_mapView->setPlacementMode(true,
        tr("Klicke auf die Map, um '%1' zu platzieren.").arg(request.nickname));
}

void SystemEditorPage::updateExclusionZonePlacementPreview(const QPointF &currentScenePos)
{
    if (!m_pendingExclusionZoneRequest || !m_pendingExclusionZoneHasCenter || !m_mapScene)
        return;

    if (!m_exclusionZonePlacementPreview) {
        const QColor stroke(220, 110, 60, 220);
        const QColor fill(220, 110, 60, 30);
        m_exclusionZonePlacementPreview = new QGraphicsEllipseItem();
        m_exclusionZonePlacementPreview->setPen(QPen(stroke, 2.0, Qt::DashLine));
        m_exclusionZonePlacementPreview->setBrush(fill);
        m_exclusionZonePlacementPreview->setZValue(10001.0);
        m_mapScene->addItem(m_exclusionZonePlacementPreview);
    }

    const QRectF rect(QPointF(m_pendingExclusionZoneCenterScenePos.x() - std::abs(currentScenePos.x() - m_pendingExclusionZoneCenterScenePos.x()),
                              m_pendingExclusionZoneCenterScenePos.y() - std::abs(currentScenePos.y() - m_pendingExclusionZoneCenterScenePos.y())),
                      QPointF(m_pendingExclusionZoneCenterScenePos.x() + std::abs(currentScenePos.x() - m_pendingExclusionZoneCenterScenePos.x()),
                              m_pendingExclusionZoneCenterScenePos.y() + std::abs(currentScenePos.y() - m_pendingExclusionZoneCenterScenePos.y())));
    m_exclusionZonePlacementPreview->setRect(rect.normalized());
}

void SystemEditorPage::finalizeExclusionZonePlacement(const QPointF &edgeScenePos)
{
    if (!m_document || !m_pendingExclusionZoneRequest)
        return;

    const QString requestedNickname = m_pendingExclusionZoneRequest->nickname.trimmed();
    static const QRegularExpression validNickname(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (requestedNickname.isEmpty() || !validNickname.match(requestedNickname).hasMatch()) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Der Nickname ist ungueltig. Bitte oeffne den Dialog erneut und pruefe den Namen."));
        cancelExclusionZonePlacement();
        return;
    }
    if (findZoneByNickname(requestedNickname) || findObjectByNickname(requestedNickname)) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Im aktuellen System existiert bereits ein Objekt oder eine Zone mit diesem Nickname."));
        cancelExclusionZonePlacement();
        return;
    }

    const QPointF centerScenePos = m_pendingExclusionZoneCenterScenePos;
    const QPointF centerFl = MapScene::qtToFl(centerScenePos.x(), centerScenePos.y());
    const QPointF edgeFl = MapScene::qtToFl(edgeScenePos.x(), edgeScenePos.y());
    const double sizeX = std::max(std::abs(edgeFl.x() - centerFl.x()), 500.0);
    const double sizeZ = std::max(std::abs(edgeFl.y() - centerFl.y()), 500.0);
    const double sizeY = std::min(sizeX, sizeZ);

    QString fieldSectionName;
    QString linkedFieldNickname;
    QString relativeFilePath;
    QString absoluteFilePath;
    if (!resolveLinkedFieldSectionForZone(m_pendingExclusionZoneRequest->fieldZoneNickname,
                                          &fieldSectionName,
                                          &linkedFieldNickname,
                                          &relativeFilePath,
                                          &absoluteFilePath)) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Die verknüpfte Felddatei konnte nicht mehr aufgelöst werden."));
        cancelExclusionZonePlacement();
        return;
    }

    const QVector3D position(static_cast<float>(centerFl.x()), 0.0f, static_cast<float>(centerFl.y()));
    const QVector3D size(static_cast<float>(sizeX), static_cast<float>(sizeY), static_cast<float>(sizeZ));
    const QVector3D rotation(0.0f, 0.0f, 0.0f);

    auto zone = std::make_shared<ZoneItem>();
    zone->setNickname(requestedNickname);
    zone->setPosition(position);
    zone->setRotation(rotation);
    zone->setSize(size);
    zone->setComment(m_pendingExclusionZoneRequest->comment);
    zone->setSortKey(m_pendingExclusionZoneRequest->sort);
    const QString shape = m_pendingExclusionZoneRequest->shape;
    if (shape == QStringLiteral("BOX"))
        zone->setShape(ZoneItem::Box);
    else if (shape == QStringLiteral("CYLINDER"))
        zone->setShape(ZoneItem::Cylinder);
    else if (shape == QStringLiteral("ELLIPSOID"))
        zone->setShape(ZoneItem::Ellipsoid);
    else
        zone->setShape(ZoneItem::Sphere);
    zone->setRawEntries(ExclusionZoneUtils::buildZoneEntries(zone->nickname(),
                                                             shape,
                                                             position,
                                                             size,
                                                             rotation,
                                                             zone->comment(),
                                                             zone->sortKey()));

    if (m_pendingExclusionZoneRequest->linkToFieldZone) {
        const QString currentText = pendingFieldIniText(relativeFilePath, absoluteFilePath);
        if (currentText.isEmpty()) {
            QMessageBox::warning(this, tr("Exclusion Zone"),
                                 tr("Die verknüpfte Felddatei konnte nicht gelesen werden."));
            cancelExclusionZonePlacement();
            return;
        }
        const auto patched = ExclusionZoneUtils::patchFieldIniExclusionSection(
            currentText,
            zone->nickname(),
            fieldSectionName.compare(QStringLiteral("Nebula"), Qt::CaseInsensitive) == 0
                ? m_pendingExclusionZoneRequest->shellSettings
                : ExclusionShellSettings{});
        stageFieldIniText(relativeFilePath, absoluteFilePath, patched.first);
    }

    m_document->addZone(zone);
    refreshObjectList();
    m_selectedNicknames = {zone->nickname()};
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateIniEditorForSelection();
    cancelExclusionZonePlacement();
}

void SystemEditorPage::finalizeSimpleZonePlacement(const QPointF &edgeScenePos)
{
    if (!m_document || !m_pendingSimpleZoneRequest)
        return;

    const QString requestedNickname = m_pendingSimpleZoneRequest->nickname.trimmed();
    static const QRegularExpression validNickname(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (requestedNickname.isEmpty() || !validNickname.match(requestedNickname).hasMatch()) {
        QMessageBox::warning(this, tr("Zone erstellen"),
                             tr("Der Nickname ist ungueltig. Bitte pruefe den Namen im Dialog."));
        cancelSimpleZonePlacement();
        return;
    }
    if (findZoneByNickname(requestedNickname) || findObjectByNickname(requestedNickname)) {
        QMessageBox::warning(this, tr("Zone erstellen"),
                             tr("Im aktuellen System existiert bereits ein Objekt oder eine Zone mit diesem Nickname."));
        cancelSimpleZonePlacement();
        return;
    }

    const QPointF centerFl = MapScene::qtToFl(m_pendingSimpleZoneCenterScenePos.x(), m_pendingSimpleZoneCenterScenePos.y());
    const QPointF edgeFl = MapScene::qtToFl(edgeScenePos.x(), edgeScenePos.y());
    const double deltaX = std::abs(edgeFl.x() - centerFl.x());
    const double deltaZ = std::abs(edgeFl.y() - centerFl.y());
    const double radius = std::max(std::max(deltaX, deltaZ), 500.0);
    const double sizeY = std::min(std::max(std::min(deltaX, deltaZ), 250.0), radius);

    auto zone = std::make_shared<ZoneItem>();
    zone->setNickname(requestedNickname);
    zone->setPosition(QVector3D(static_cast<float>(centerFl.x()), 0.0f, static_cast<float>(centerFl.y())));
    zone->setRotation(QVector3D());
    zone->setComment(m_pendingSimpleZoneRequest->comment);
    zone->setDamage(m_pendingSimpleZoneRequest->damage);
    zone->setSortKey(m_pendingSimpleZoneRequest->sort);

    QString sizeText;
    const QString shape = m_pendingSimpleZoneRequest->shape.toUpper();
    if (shape == QStringLiteral("ELLIPSOID")) {
        zone->setShape(ZoneItem::Ellipsoid);
        zone->setSize(QVector3D(static_cast<float>(deltaX), static_cast<float>(sizeY), static_cast<float>(deltaZ)));
        sizeText = QStringLiteral("%1, %2, %3")
                       .arg(std::max(deltaX, 500.0), 0, 'f', 0)
                       .arg(sizeY, 0, 'f', 0)
                       .arg(std::max(deltaZ, 500.0), 0, 'f', 0);
    } else if (shape == QStringLiteral("BOX")) {
        zone->setShape(ZoneItem::Box);
        zone->setSize(QVector3D(static_cast<float>(deltaX), static_cast<float>(sizeY), static_cast<float>(deltaZ)));
        sizeText = QStringLiteral("%1, %2, %3")
                       .arg(std::max(deltaX, 500.0), 0, 'f', 0)
                       .arg(sizeY, 0, 'f', 0)
                       .arg(std::max(deltaZ, 500.0), 0, 'f', 0);
    } else if (shape == QStringLiteral("CYLINDER")) {
        const double length = std::max(std::max(deltaX, deltaZ) * 2.0, 1000.0);
        zone->setShape(ZoneItem::Cylinder);
        zone->setSize(QVector3D(static_cast<float>(radius), static_cast<float>(length), static_cast<float>(radius)));
        sizeText = QStringLiteral("%1, %2")
                       .arg(radius, 0, 'f', 0)
                       .arg(length, 0, 'f', 0);
    } else if (shape == QStringLiteral("RING")) {
        const double length = std::max(std::max(deltaX, deltaZ) * 2.0, 1000.0);
        zone->setShape(ZoneItem::Ring);
        zone->setSize(QVector3D(static_cast<float>(radius), static_cast<float>(length), static_cast<float>(radius)));
        sizeText = QStringLiteral("%1, %2")
                       .arg(radius, 0, 'f', 0)
                       .arg(length, 0, 'f', 0);
    } else {
        zone->setShape(ZoneItem::Sphere);
        zone->setSize(QVector3D(static_cast<float>(radius), 0.0f, 0.0f));
        sizeText = QString::number(radius, 'f', 0);
    }

    QVector<QPair<QString, QString>> entries{
        {QStringLiteral("nickname"), requestedNickname},
        {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(centerFl.x(), 0, 'f', 0).arg(centerFl.y(), 0, 'f', 0)},
        {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
        {QStringLiteral("shape"), shape},
        {QStringLiteral("size"), sizeText},
        {QStringLiteral("sort"), QString::number(m_pendingSimpleZoneRequest->sort)},
    };
    if (m_pendingSimpleZoneRequest->damage > 0)
        entries.append({QStringLiteral("damage"), QString::number(m_pendingSimpleZoneRequest->damage)});
    zone->setRawEntries(entries);

    auto *cmd = new AddZoneCommand(m_document.get(), zone, tr("Create Zone"));
    flatlas::core::UndoManager::instance().push(cmd);
    refreshObjectList();
    m_selectedNicknames = {zone->nickname()};
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateIniEditorForSelection();
    cancelSimpleZonePlacement();
}

void SystemEditorPage::finalizePatrolZonePlacement(const QPointF &endScenePos)
{
    if (!m_document || !m_pendingPatrolZoneRequest)
        return;

    const CreatePatrolZoneRequest request = *m_pendingPatrolZoneRequest;
    static const QRegularExpression validNickname(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (request.nickname.trimmed().isEmpty() || !validNickname.match(request.nickname.trimmed()).hasMatch()) {
        QMessageBox::warning(this, tr("Patrol-Zone erstellen"),
                             tr("Der Nickname ist ungueltig. Bitte pruefe den Namen im Dialog."));
        cancelPatrolZonePlacement();
        return;
    }
    if (findZoneByNickname(request.nickname) || findObjectByNickname(request.nickname)) {
        QMessageBox::warning(this, tr("Patrol-Zone erstellen"),
                             tr("Im aktuellen System existiert bereits ein Objekt oder eine Zone mit diesem Nickname."));
        cancelPatrolZonePlacement();
        return;
    }

    const QString factionNickname = factionNicknameFromDisplay(request.factionDisplay);
    if (request.encounter.trimmed().isEmpty() || factionNickname.isEmpty()) {
        QMessageBox::warning(this, tr("Patrol-Zone erstellen"),
                             tr("Encounter und Faction muessen gesetzt sein."));
        cancelPatrolZonePlacement();
        return;
    }

    QString encounterError;
    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    if (!ensureEncounterParameterExists(m_document.get(), request.encounter, gameRoot, &encounterError)) {
        QMessageBox::warning(this, tr("Patrol-Zone erstellen"), encounterError);
        cancelPatrolZonePlacement();
        return;
    }

    if (!m_pendingPatrolZoneHasEnd) {
        cancelPatrolZonePlacement();
        return;
    }

    Q_UNUSED(endScenePos);

    const QPointF startFl = MapScene::qtToFl(m_pendingPatrolZoneStartScenePos.x(), m_pendingPatrolZoneStartScenePos.y());
    const QPointF endFl = MapScene::qtToFl(m_pendingPatrolZoneEndScenePos.x(), m_pendingPatrolZoneEndScenePos.y());
    const double dx = endFl.x() - startFl.x();
    const double dz = endFl.y() - startFl.y();
    const double length = std::max(std::hypot(dx, dz), 1000.0);
    const QPointF centerFl((startFl.x() + endFl.x()) * 0.5, (startFl.y() + endFl.y()) * 0.5);
    const double yaw = patrolYawDegrees(startFl, endFl);
    const double radius = std::max(static_cast<double>(m_pendingPatrolZoneHalfWidthScene) * 100.0, 100.0);
    const QString usage = request.usage.trimmed().isEmpty() ? QStringLiteral("patrol") : request.usage.trimmed().toLower();

    auto zone = std::make_shared<ZoneItem>();
    zone->setNickname(request.nickname);
    zone->setPosition(QVector3D(static_cast<float>(centerFl.x()), 0.0f, static_cast<float>(centerFl.y())));
    zone->setRotation(QVector3D(90.0f, static_cast<float>(yaw), 0.0f));
    zone->setShape(ZoneItem::Cylinder);
    zone->setSize(QVector3D(static_cast<float>(radius), static_cast<float>(length), static_cast<float>(radius)));
    zone->setComment(request.comment);
    zone->setDamage(request.damage);
    zone->setSortKey(request.sort);
    zone->setUsage(usage);
    zone->setPopType(request.popType);
    zone->setPathLabel(QStringLiteral("%1, %2").arg(request.pathLabel.isEmpty() ? QStringLiteral("patrol") : request.pathLabel)
                           .arg(request.pathIndex));

    QVector<QPair<QString, QString>> entries{
        {QStringLiteral("nickname"), request.nickname},
        {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(centerFl.x(), 0, 'f', 0).arg(centerFl.y(), 0, 'f', 0)},
        {QStringLiteral("rotate"), QStringLiteral("90, %1, 0").arg(yaw, 0, 'f', 6)},
        {QStringLiteral("shape"), QStringLiteral("CYLINDER")},
        {QStringLiteral("size"),
         QStringLiteral("%1, %2, %3")
             .arg(radius, 0, 'f', 6)
             .arg(length, 0, 'f', 6)
             .arg(radius, 0, 'f', 6)},
        {QStringLiteral("sort"), QString::number(request.sort)},
        {QStringLiteral("toughness"), QString::number(request.toughness)},
        {QStringLiteral("density"), QString::number(request.density)},
        {QStringLiteral("repop_time"), QString::number(request.repopTime)},
        {QStringLiteral("max_battle_size"), QString::number(request.maxBattleSize)},
        {QStringLiteral("pop_type"), request.popType},
        {QStringLiteral("relief_time"), QString::number(request.reliefTime)},
        {QStringLiteral("path_label"), QStringLiteral("%1, %2")
                                           .arg(request.pathLabel.isEmpty() ? QStringLiteral("patrol") : request.pathLabel)
                                           .arg(request.pathIndex)},
        {QStringLiteral("usage"), usage},
        {QStringLiteral("mission_eligible"), request.missionEligible ? QStringLiteral("true") : QStringLiteral("false")},
    };
    if (request.damage > 0)
        entries.append({QStringLiteral("damage"), QString::number(request.damage)});
    const QStringList restrictions = patrolDensityRestrictionsForUsage(usage);
    for (const QString &restriction : restrictions)
        entries.append({QStringLiteral("density_restriction"), restriction});
    entries.append({QStringLiteral("encounter"),
                    QStringLiteral("%1, %2, %3")
                        .arg(request.encounter)
                        .arg(request.encounterLevel)
                        .arg(QString::number(request.encounterChance, 'f', 2))});
    entries.append({QStringLiteral("faction"), QStringLiteral("%1, 1").arg(factionNickname)});
    zone->setRawEntries(entries);

    auto *cmd = new AddZoneCommand(m_document.get(), zone, tr("Create Patrol Zone"));
    flatlas::core::UndoManager::instance().push(cmd);
    refreshObjectList();
    m_selectedNicknames = {zone->nickname()};
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateIniEditorForSelection();
    cancelPatrolZonePlacement();
}

void SystemEditorPage::finalizeBuoyPlacement(const QPointF &scenePos)
{
    if (!m_document || !m_pendingBuoyRequest || !m_pendingBuoyHasAnchor)
        return;

    const QString archetype = m_pendingBuoyRequest->archetype.trimmed().toLower();
    if (archetype.isEmpty()) {
        QMessageBox::warning(this, tr("Bojen erstellen"),
                             tr("Es wurde kein gueltiger Bojentyp ausgewaehlt."));
        cancelBuoyPlacement();
        return;
    }

    QVector<QPointF> positionsScene;
    const int count = std::max(m_pendingBuoyRequest->count,
                               m_pendingBuoyRequest->mode == CreateBuoyRequest::Mode::Line ? 2 : 3);
    if (m_pendingBuoyRequest->mode == CreateBuoyRequest::Mode::Line) {
        const QLineF directionLine(m_pendingBuoyAnchorScenePos, scenePos);
        const qreal directionLength = directionLine.length();
        if (directionLength <= 0.001) {
            QMessageBox::warning(this, tr("Bojen erstellen"),
                                 tr("Die Richtung fuer die Bojenlinie ist ungueltig."));
            m_mapView->setPlacementMode(true,
                                        tr("2. Klick bestimmt die Richtung fuer das Bojenmuster. [Esc] oder Rechtsklick bricht ab."));
            return;
        }

        int finalCount = std::max(m_pendingBuoyRequest->count, 2);
        qreal spacingScene = std::max<qreal>(static_cast<qreal>(m_pendingBuoyRequest->spacingMeters) / 100.0, 1.0);
        if (m_pendingBuoyRequest->lineConstraint == CreateBuoyRequest::LineConstraint::FixedCount) {
            spacingScene = std::max<qreal>(directionLength / std::max(finalCount - 1, 1), 1.0);
        } else {
            finalCount = derivedBuoyCountForLine(directionLength, m_pendingBuoyRequest->spacingMeters);
        }
        const QPointF unit((scenePos.x() - m_pendingBuoyAnchorScenePos.x()) / directionLength,
                           (scenePos.y() - m_pendingBuoyAnchorScenePos.y()) / directionLength);
        positionsScene.reserve(finalCount);
        for (int index = 0; index < finalCount; ++index) {
            positionsScene.append(m_pendingBuoyAnchorScenePos
                                  + QPointF(unit.x() * spacingScene * index,
                                            unit.y() * spacingScene * index));
        }
    } else {
        const qreal radius = QLineF(m_pendingBuoyAnchorScenePos, scenePos).length();
        if (radius <= 0.001) {
            QMessageBox::warning(this, tr("Bojen erstellen"),
                                 tr("Der Radius fuer den Bojenkreis ist ungueltig."));
            m_mapView->setPlacementMode(true,
                                        tr("2. Klick bestimmt den Radius fuer das Bojenmuster. [Esc] oder Rechtsklick bricht ab."));
            return;
        }

        positionsScene.reserve(count);
        for (int index = 0; index < count; ++index) {
            const double angle = (2.0 * M_PI * static_cast<double>(index)) / static_cast<double>(count);
            positionsScene.append(QPointF(m_pendingBuoyAnchorScenePos.x() + std::cos(angle) * radius,
                                          m_pendingBuoyAnchorScenePos.y() + std::sin(angle) * radius));
        }
    }

    QStringList existingNicknames;
    existingNicknames.reserve(m_document->objects().size());
    for (const auto &obj : m_document->objects()) {
        if (obj)
            existingNicknames.append(obj->nickname());
    }
    QSet<QString> usedNicknames;
    for (const QString &nickname : std::as_const(existingNicknames))
        usedNicknames.insert(nickname.toLower());

    const QString prefix = QStringLiteral("%1_%2_")
                               .arg(buoyNicknameSystemToken(m_document->filePath()))
                               .arg(archetype);
    const int idsName = buoyIdsNameForArchetype(archetype);
    const int idsInfo = buoyIdsInfoForArchetype(archetype);

    auto *stack = flatlas::core::UndoManager::instance().stack();
    stack->beginMacro(tr("Create Buoys"));

    QStringList createdNicknames;
    createdNicknames.reserve(positionsScene.size());
    for (const QPointF &scenePoint : std::as_const(positionsScene)) {
        const QPointF worldXZ = MapScene::qtToFl(scenePoint.x(), scenePoint.y());
        auto obj = std::make_shared<SolarObject>();
        const QString nickname = nextIndexedNickname(prefix, &usedNicknames);
        obj->setNickname(nickname);
        obj->setType(SolarObject::Waypoint);
        obj->setArchetype(archetype);
        obj->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        if (idsName > 0)
            obj->setIdsName(idsName);
        if (idsInfo > 0)
            obj->setIdsInfo(idsInfo);

        QVector<QPair<QString, QString>> rawEntries{
            {QStringLiteral("nickname"), nickname},
            {QStringLiteral("pos"),
             QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("archetype"), archetype},
        };
        if (idsName > 0)
            rawEntries.append({QStringLiteral("ids_name"), QString::number(idsName)});
        if (idsInfo > 0)
            rawEntries.append({QStringLiteral("ids_info"), QString::number(idsInfo)});
        obj->setRawEntries(rawEntries);

        stack->push(new AddObjectCommand(m_document.get(), obj, tr("Create Buoys")));
        createdNicknames.append(nickname);
    }
    stack->endMacro();

    refreshObjectList();
    m_selectedNicknames = createdNicknames;
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
    cancelBuoyPlacement();
}

void SystemEditorPage::finalizeTradeLanePlacement(const QPointF &endScenePos)
{
    if (!m_document || !m_pendingTradeLaneHasStart || !m_mapView)
        return;

    const QLineF laneLine(m_pendingTradeLaneStartScenePos, endScenePos);
    const qreal laneLengthScene = laneLine.length();
    if (laneLengthScene <= 0.001) {
        QMessageBox::warning(this, tr("Trade Lane erstellen"),
                             tr("Start- und Endpunkt der Trade Lane sind ungueltig."));
        m_mapView->setPlacementMode(true,
                                    tr("2. Klick setzt das Ende der Trade Lane. [Esc] oder Rechtsklick bricht ab."));
        return;
    }

    if (m_tradeLanePlacementPreview && m_mapScene)
        m_tradeLanePlacementPreview->setLine(laneLine);

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const QString systemNickname = QFileInfo(m_document->filePath()).completeBaseName();
    CreateTradeLaneDialog dialog(systemNickname,
                                 nextTradeLaneRingNumber(m_document.get(), systemNickname),
                                 std::max(2, qRound((laneLengthScene * 100.0) / 7500.0) + 1),
                                 laneLengthScene * 100.0,
                                 loadLoadoutsMatching(gameRoot, {QStringLiteral("trade_lane_ring")}),
                                 loadFactionDisplays(gameRoot),
                                 loadPilotNicknames(gameRoot),
                                 this);
    if (dialog.exec() != QDialog::Accepted) {
        cancelTradeLanePlacement();
        return;
    }

    const CreateTradeLaneRequest request = dialog.result();
    if (request.ringCount < 2) {
        QMessageBox::warning(this, tr("Trade Lane erstellen"),
                             tr("Es muessen mindestens zwei Trade-Lane-Ringe erstellt werden."));
        cancelTradeLanePlacement();
        return;
    }
    if (request.loadout.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Trade Lane erstellen"),
                             tr("Bitte waehle ein gueltiges Loadout fuer die Trade Lane."));
        cancelTradeLanePlacement();
        return;
    }

    QSet<QString> usedNicknames;
    for (const auto &obj : m_document->objects()) {
        if (obj)
            usedNicknames.insert(obj->nickname().trimmed().toLower());
    }

    QStringList plannedNicknames;
    plannedNicknames.reserve(request.ringCount);
    for (int index = 0; index < request.ringCount; ++index) {
        const QString nickname =
            QStringLiteral("%1_Trade_Lane_Ring_%2").arg(systemNickname).arg(request.startNumber + index);
        const QString key = nickname.trimmed().toLower();
        if (usedNicknames.contains(key)) {
            QMessageBox::warning(this, tr("Trade Lane erstellen"),
                                 tr("Der Ring-Nickname '%1' existiert bereits. Bitte waehle eine andere Startnummer.")
                                     .arg(nickname));
            cancelTradeLanePlacement();
            return;
        }
        usedNicknames.insert(key);
        plannedNicknames.append(nickname);
    }

    int routeIdsName = 0;
    int startSpaceNameId = 0;
    int endSpaceNameId = 0;
    QString idsError;
    if (!resolveIdsStringInput(gameRoot, request.routeName, 0, QString(), &routeIdsName, &idsError)
        || !resolveIdsStringInput(gameRoot, request.startSpaceName, 0, QString(), &startSpaceNameId, &idsError)
        || !resolveIdsStringInput(gameRoot, request.endSpaceName, 0, QString(), &endSpaceNameId, &idsError)) {
        QMessageBox::warning(this, tr("Trade Lane erstellen"),
                             idsError.trimmed().isEmpty()
                                 ? tr("Die IDS-Texte fuer die Trade Lane konnten nicht geschrieben werden.")
                                 : tr("Die IDS-Texte fuer die Trade Lane konnten nicht geschrieben werden:\n%1")
                                       .arg(idsError));
        cancelTradeLanePlacement();
        return;
    }

    const QString reputation = factionNicknameFromDisplay(request.reputationDisplay);
    const double yawDegrees = tradeLaneYawDegrees(m_pendingTradeLaneStartScenePos, endScenePos);

    auto *stack = flatlas::core::UndoManager::instance().stack();
    stack->beginMacro(tr("Trade Lane erstellen"));

    QStringList createdNicknames;
    createdNicknames.reserve(request.ringCount);
    for (int index = 0; index < request.ringCount; ++index) {
        const double t = request.ringCount <= 1
            ? 0.0
            : static_cast<double>(index) / static_cast<double>(request.ringCount - 1);
        const QPointF scenePos(
            m_pendingTradeLaneStartScenePos.x()
                + (endScenePos.x() - m_pendingTradeLaneStartScenePos.x()) * t,
            m_pendingTradeLaneStartScenePos.y()
                + (endScenePos.y() - m_pendingTradeLaneStartScenePos.y()) * t);
        const QPointF worldXZ = MapScene::qtToFl(scenePos.x(), scenePos.y());

        auto obj = std::make_shared<SolarObject>();
        const QString nickname = plannedNicknames[index];
        obj->setNickname(nickname);
        obj->setType(SolarObject::TradeLane);
        obj->setArchetype(QStringLiteral("Trade_Lane_Ring"));
        obj->setPosition(QVector3D(static_cast<float>(worldXZ.x()), 0.0f, static_cast<float>(worldXZ.y())));
        obj->setRotation(QVector3D(0.0f, static_cast<float>(yawDegrees), 0.0f));
        obj->setIdsInfo(66170);
        obj->setLoadout(request.loadout);
        if (routeIdsName > 0)
            obj->setIdsName(routeIdsName);

        QVector<QPair<QString, QString>> entries{
            {QStringLiteral("nickname"), nickname},
            {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(worldXZ.x(), 0, 'f', 0).arg(worldXZ.y(), 0, 'f', 0)},
            {QStringLiteral("rotate"), QStringLiteral("0, %1, 0").arg(yawDegrees, 0, 'f', 0)},
            {QStringLiteral("archetype"), QStringLiteral("Trade_Lane_Ring")},
            {QStringLiteral("ids_name"), QString::number(routeIdsName)},
        };
        if (index > 0)
            entries.append({QStringLiteral("prev_ring"), plannedNicknames[index - 1]});
        if (index < request.ringCount - 1)
            entries.append({QStringLiteral("next_ring"), plannedNicknames[index + 1]});
        entries.append({QStringLiteral("ids_info"), QStringLiteral("66170")});
        if (!reputation.isEmpty())
            entries.append({QStringLiteral("reputation"), reputation});
        if (index == 0 && startSpaceNameId > 0)
            entries.append({QStringLiteral("tradelane_space_name"), QString::number(startSpaceNameId)});
        else if (index == request.ringCount - 1 && endSpaceNameId > 0)
            entries.append({QStringLiteral("tradelane_space_name"), QString::number(endSpaceNameId)});
        entries.append({QStringLiteral("behavior"), QStringLiteral("NOTHING")});
        entries.append({QStringLiteral("difficulty_level"), QString::number(request.difficultyLevel)});
        entries.append({QStringLiteral("loadout"), request.loadout});
        if (!request.pilot.trimmed().isEmpty())
            entries.append({QStringLiteral("pilot"), request.pilot.trimmed()});
        obj->setRawEntries(entries);

        stack->push(new AddObjectCommand(m_document.get(), obj, tr("Create Trade Lane")));
        createdNicknames.append(nickname);
    }
    stack->endMacro();

    refreshObjectList();
    m_selectedNicknames = createdNicknames;
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
    cancelTradeLanePlacement();
}

void SystemEditorPage::cancelFieldZonePlacement()
{
    clearFieldZonePlacementPreview();
    if (m_mapView && m_mapView->viewport()) {
        m_mapView->setPlacementMode(false);
        m_mapView->viewport()->removeEventFilter(this);
        m_mapView->viewport()->setToolTip(QString());
    }
    m_pendingFieldZoneRequest.reset();
    m_pendingFieldZoneHasCenter = false;
    m_pendingFieldZoneCenterScenePos = QPointF();
}

void SystemEditorPage::cancelSimpleZonePlacement()
{
    if (m_simpleZonePlacementPreview && m_mapScene) {
        m_mapScene->removeItem(m_simpleZonePlacementPreview);
        delete m_simpleZonePlacementPreview;
        m_simpleZonePlacementPreview = nullptr;
    }
    if (m_mapView && m_mapView->viewport()) {
        m_mapView->setPlacementMode(false);
        m_mapView->viewport()->removeEventFilter(this);
        m_mapView->viewport()->setToolTip(QString());
    }
    m_pendingSimpleZoneRequest.reset();
    m_pendingSimpleZoneHasCenter = false;
    m_pendingSimpleZoneCenterScenePos = QPointF();
}

void SystemEditorPage::cancelPatrolZonePlacement()
{
    if (m_patrolZonePlacementPreview && m_mapScene) {
        m_mapScene->removeItem(m_patrolZonePlacementPreview);
        delete m_patrolZonePlacementPreview;
        m_patrolZonePlacementPreview = nullptr;
    }
    if (m_patrolZoneWidthPreview && m_mapScene) {
        m_mapScene->removeItem(m_patrolZoneWidthPreview);
        delete m_patrolZoneWidthPreview;
        m_patrolZoneWidthPreview = nullptr;
    }
    if (m_mapView && m_mapView->viewport()) {
        m_mapView->setPlacementMode(false);
        m_mapView->viewport()->removeEventFilter(this);
        m_mapView->viewport()->setToolTip(QString());
    }
    m_pendingPatrolZoneRequest.reset();
    m_pendingPatrolZoneHasStart = false;
    m_pendingPatrolZoneStartScenePos = QPointF();
    m_pendingPatrolZoneHasEnd = false;
    m_pendingPatrolZoneEndScenePos = QPointF();
    m_pendingPatrolZoneHalfWidthScene = 0.0;
    m_pendingPatrolZoneStep = 0;
}

void SystemEditorPage::cancelBuoyPlacement()
{
    if (m_buoyLinePreview && m_mapScene) {
        m_mapScene->removeItem(m_buoyLinePreview);
        delete m_buoyLinePreview;
        m_buoyLinePreview = nullptr;
    }
    if (m_buoyCirclePreview && m_mapScene) {
        m_mapScene->removeItem(m_buoyCirclePreview);
        delete m_buoyCirclePreview;
        m_buoyCirclePreview = nullptr;
    }
    for (QGraphicsEllipseItem *marker : std::as_const(m_buoyMarkerPreviews)) {
        if (marker && m_mapScene)
            m_mapScene->removeItem(marker);
        delete marker;
    }
    m_buoyMarkerPreviews.clear();
    if (m_mapView && m_mapView->viewport()) {
        m_mapView->setPlacementMode(false);
        m_mapView->viewport()->removeEventFilter(this);
        m_mapView->viewport()->setToolTip(QString());
    }
    m_pendingBuoyRequest.reset();
    m_pendingBuoyHasAnchor = false;
    m_pendingBuoyAnchorScenePos = QPointF();
    m_pendingBuoyStep = 0;
}

void SystemEditorPage::cancelTradeLanePlacement()
{
    if (m_tradeLanePlacementPreview && m_mapScene) {
        m_mapScene->removeItem(m_tradeLanePlacementPreview);
        delete m_tradeLanePlacementPreview;
        m_tradeLanePlacementPreview = nullptr;
    }
    if (m_mapView && m_mapView->viewport()) {
        m_mapView->setPlacementMode(false);
        m_mapView->viewport()->removeEventFilter(this);
        m_mapView->viewport()->setToolTip(QString());
    }
    m_pendingTradeLaneHasStart = false;
    m_pendingTradeLaneStartScenePos = QPointF();
}

void SystemEditorPage::cancelExclusionZonePlacement()
{
    if (m_exclusionZonePlacementPreview && m_mapScene) {
        m_mapScene->removeItem(m_exclusionZonePlacementPreview);
        delete m_exclusionZonePlacementPreview;
        m_exclusionZonePlacementPreview = nullptr;
    }
    if (m_mapView && m_mapView->viewport()) {
        m_mapView->setPlacementMode(false);
        m_mapView->viewport()->removeEventFilter(this);
    }
    m_pendingExclusionZoneRequest.reset();
    m_pendingExclusionZoneHasCenter = false;
    m_pendingExclusionZoneCenterScenePos = QPointF();
}

void SystemEditorPage::clearFieldZonePlacementPreview()
{
    if (!m_fieldZonePlacementPreview || !m_mapScene)
        return;
    m_mapScene->removeItem(m_fieldZonePlacementPreview);
    delete m_fieldZonePlacementPreview;
    m_fieldZonePlacementPreview = nullptr;
}

void SystemEditorPage::addLinkedFieldSection(const QString &sectionName,
                                             const QString &zoneNickname,
                                             const QString &relativeFilePath)
{
    if (!m_document)
        return;

    IniDocument extras = SystemPersistence::extraSections(m_document.get());
    IniSection linkedSection;
    linkedSection.name = sectionName;
    linkedSection.entries.append({QStringLiteral("file"), relativeFilePath});
    linkedSection.entries.append({QStringLiteral("zone"), zoneNickname});
    extras.append(linkedSection);
    SystemPersistence::setExtraSections(m_document.get(), extras);
    refreshDocumentDirtyState();
}

void SystemEditorPage::removeLinkedFieldSectionsForNicknames(const QStringList &zoneNicknames)
{
    if (!m_document || zoneNicknames.isEmpty())
        return;

    IniDocument extras = SystemPersistence::extraSections(m_document.get());
    IniDocument filtered;
    filtered.reserve(extras.size());

    for (const IniSection &section : std::as_const(extras)) {
        const bool isLinkedFieldSection =
            section.name.compare(QStringLiteral("Asteroids"), Qt::CaseInsensitive) == 0
            || section.name.compare(QStringLiteral("Nebula"), Qt::CaseInsensitive) == 0;
        if (!isLinkedFieldSection) {
            filtered.append(section);
            continue;
        }

        const QString linkedZone = section.value(QStringLiteral("zone")).trimmed();
        bool shouldRemove = false;
        for (const QString &nickname : zoneNicknames) {
            if (linkedZone.compare(nickname, Qt::CaseInsensitive) == 0) {
                shouldRemove = true;
                break;
            }
        }
        if (shouldRemove) {
            const QString relativeFile = section.value(QStringLiteral("file")).trimmed().toLower();
            m_pendingGeneratedZoneFiles.remove(relativeFile);
            continue;
        }
        filtered.append(section);
    }

    SystemPersistence::setExtraSections(m_document.get(), filtered);
    refreshDocumentDirtyState();
}

bool SystemEditorPage::writePendingGeneratedZoneFiles(QString *errorMessage)
{
    for (auto it = m_pendingGeneratedZoneFiles.constBegin(); it != m_pendingGeneratedZoneFiles.constEnd(); ++it) {
        const PendingGeneratedZoneFile &fileData = it.value();
        if (fileData.absolutePath.trimmed().isEmpty())
            continue;

        QDir().mkpath(QFileInfo(fileData.absolutePath).absolutePath());
        QSaveFile file(fileData.absolutePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            if (errorMessage) {
                *errorMessage = tr("Die generierte Referenzdatei konnte nicht geschrieben werden:\n%1")
                                    .arg(fileData.absolutePath);
            }
            return false;
        }
        file.write(fileData.content.toUtf8());
        if (!file.commit()) {
            if (errorMessage) {
                *errorMessage = tr("Die generierte Referenzdatei konnte nicht geschrieben werden:\n%1")
                                    .arg(fileData.absolutePath);
            }
            return false;
        }
    }
    return true;
}

bool SystemEditorPage::writePendingTextFiles(QString *errorMessage)
{
    for (auto it = m_pendingTextFileWrites.constBegin(); it != m_pendingTextFileWrites.constEnd(); ++it) {
        const PendingTextFileWrite &fileData = it.value();
        if (fileData.absolutePath.trimmed().isEmpty())
            continue;
        QDir().mkpath(QFileInfo(fileData.absolutePath).absolutePath());
        QSaveFile file(fileData.absolutePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            if (errorMessage)
                *errorMessage = tr("Die verknuepfte Datei konnte nicht geschrieben werden:\n%1")
                                    .arg(fileData.absolutePath);
            return false;
        }
        file.write(fileData.content.toUtf8());
        if (!file.commit()) {
            if (errorMessage)
                *errorMessage = tr("Die verknuepfte Datei konnte nicht geschrieben werden:\n%1")
                                    .arg(fileData.absolutePath);
            return false;
        }
    }
    return true;
}

void SystemEditorPage::stagePendingTextWrite(const QString &absolutePath, const QString &content)
{
    const QString key = QDir::cleanPath(absolutePath).toLower();
    m_pendingTextFileWrites.insert(key, PendingTextFileWrite{absolutePath, content});
    refreshDocumentDirtyState();
}

SolarObject *SystemEditorPage::findObjectByNickname(const QString &nickname) const
{
    if (!m_document)
        return nullptr;
    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname)
            return obj.get();
    }
    return nullptr;
}

ZoneItem *SystemEditorPage::findZoneByNickname(const QString &nickname) const
{
    if (!m_document)
        return nullptr;
    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname)
            return zone.get();
    }
    return nullptr;
}

SolarObject *SystemEditorPage::findBaseHostForSelection() const
{
    if (!m_document || m_selectedNicknames.size() != 1)
        return nullptr;

    SolarObject *object = findObjectByNickname(primarySelectedNickname());
    if (!object)
        return nullptr;
    return BaseEditService::objectHasBase(*object) ? object : nullptr;
}

int SystemEditorPage::findLightSourceSectionIndexByNickname(const QString &nickname) const
{
    if (!m_document)
        return -1;
    const IniDocument extras = SystemPersistence::extraSections(m_document.get());
    for (int index = 0; index < extras.size(); ++index) {
        if (extras[index].name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) != 0)
            continue;
        if (extras[index].value(QStringLiteral("nickname")).trimmed().compare(nickname.trimmed(), Qt::CaseInsensitive) == 0)
            return index;
    }
    return -1;
}

QStringList SystemEditorPage::lightSourceNicknames() const
{
    QStringList values;
    if (!m_document)
        return values;
    const IniDocument extras = SystemPersistence::extraSections(m_document.get());
    for (const IniSection &section : extras) {
        if (section.name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (!nickname.isEmpty())
            values.append(nickname);
    }
    return values;
}

QString SystemEditorPage::normalizeObjectNicknameToGroupRoot(const QString &nickname) const
{
    SolarObject *object = findObjectByNickname(nickname);
    if (!object)
        return nickname;

    QString currentNickname = object->nickname();
    QSet<QString> visited;
    while (object) {
        if (visited.contains(currentNickname))
            break;
        visited.insert(currentNickname);

        const QString parentNickname = parentNicknameForObject(*object);
        if (parentNickname.isEmpty())
            break;

        SolarObject *parentObject = findObjectByNickname(parentNickname);
        if (!parentObject)
            break;

        currentNickname = parentObject->nickname();
        object = parentObject;
    }

    return currentNickname;
}

QStringList SystemEditorPage::normalizeSelectionNicknames(const QStringList &nicknames) const
{
    QStringList normalized;
    normalized.reserve(nicknames.size());
    for (const QString &nickname : nicknames) {
        if (findObjectByNickname(nickname))
            normalized.append(normalizeObjectNicknameToGroupRoot(nickname));
        else if (findZoneByNickname(nickname))
            normalized.append(nickname);
        else if (findLightSourceSectionIndexByNickname(nickname) >= 0)
            normalized.append(nickname);
    }
    normalized.removeDuplicates();
    return normalized;
}

QStringList SystemEditorPage::expandSelectionNicknamesForScene(const QStringList &nicknames) const
{
    QStringList expanded;
    for (const QString &nickname : nicknames) {
        if (findObjectByNickname(nickname))
            expanded.append(objectGroupNicknames(normalizeObjectNicknameToGroupRoot(nickname)));
        else if (findZoneByNickname(nickname))
            expanded.append(nickname);
        else if (findLightSourceSectionIndexByNickname(nickname) >= 0)
            expanded.append(nickname);
    }
    expanded.removeDuplicates();
    return expanded;
}

QStringList SystemEditorPage::expandMoveNicknames(const QStringList &nicknames) const
{
    QStringList expanded;
    for (const QString &nickname : nicknames) {
        SolarObject *object = findObjectByNickname(nickname);
        if (object) {
            const QString rootNickname = normalizeObjectNicknameToGroupRoot(nickname);
            const QStringList groupNicknames = objectGroupNicknames(rootNickname);
            expanded.append(groupNicknames);
            for (const QString &groupNickname : groupNicknames) {
                SolarObject *groupObject = findObjectByNickname(groupNickname);
                if (!groupObject || !isPlanetLikeObject(*groupObject))
                    continue;

                const QString deathZoneNickname = QStringLiteral("Zone_%1_death").arg(groupObject->nickname());
                if (findZoneByNickname(deathZoneNickname))
                    expanded.append(deathZoneNickname);

                const QString ringZoneNickname = RingEditService::linkedZoneNickname(*groupObject);
                if (!ringZoneNickname.isEmpty() && findZoneByNickname(ringZoneNickname))
                    expanded.append(ringZoneNickname);
            }
            continue;
        }

        if (ZoneItem *zone = findZoneByNickname(nickname)) {
            expanded.append(zone->nickname());
            if (SolarObject *hostObject = RingEditService::findHostForZone(m_document.get(), zone->nickname())) {
                const QString rootNickname = normalizeObjectNicknameToGroupRoot(hostObject->nickname());
                const QStringList groupNicknames = objectGroupNicknames(rootNickname);
                expanded.append(groupNicknames);
                for (const QString &groupNickname : groupNicknames) {
                    SolarObject *groupObject = findObjectByNickname(groupNickname);
                    if (!groupObject || !isPlanetLikeObject(*groupObject))
                        continue;

                    const QString deathZoneNickname = QStringLiteral("Zone_%1_death").arg(groupObject->nickname());
                    if (findZoneByNickname(deathZoneNickname))
                        expanded.append(deathZoneNickname);

                    const QString ringZoneNickname = RingEditService::linkedZoneNickname(*groupObject);
                    if (!ringZoneNickname.isEmpty() && findZoneByNickname(ringZoneNickname))
                        expanded.append(ringZoneNickname);
                }
            }
        } else if (findLightSourceSectionIndexByNickname(nickname) >= 0) {
            expanded.append(nickname);
        }
    }
    expanded.removeDuplicates();
    return expanded;
}

void SystemEditorPage::syncLightSourcesInScene()
{
    if (!m_mapScene) {
        return;
    }

    QVector<MapScene::LightSourceVisual> lightSources;
    if (m_document) {
        const IniDocument extras = SystemPersistence::extraSections(m_document.get());
        for (const IniSection &section : extras) {
            if (section.name.compare(QStringLiteral("LightSource"), Qt::CaseInsensitive) != 0)
                continue;

            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;

            QVector3D position;
            if (!parseIniVector3(section.value(QStringLiteral("pos")).trimmed(), &position))
                continue;

            MapScene::LightSourceVisual visual;
            visual.nickname = nickname;
            visual.position = position;
            lightSources.append(visual);
        }
    }

    m_mapScene->setLightSources(lightSources);
}

QStringList SystemEditorPage::objectGroupNicknames(const QString &rootNickname) const
{
    if (!m_document)
        return {};

    const QString canonicalRoot = normalizeObjectNicknameToGroupRoot(rootNickname);
    if (!findObjectByNickname(canonicalRoot))
        return {};

    QStringList ordered{canonicalRoot};
    bool appended = true;
    while (appended) {
        appended = false;
        for (const auto &obj : m_document->objects()) {
            const QString parentNickname = parentNicknameForObject(*obj);
            if (parentNickname.isEmpty() || ordered.contains(obj->nickname()))
                continue;
            if (ordered.contains(parentNickname)) {
                ordered.append(obj->nickname());
                appended = true;
            }
        }
    }
    return ordered;
}

bool SystemEditorPage::isPlanetLikeObject(const SolarObject &obj) const
{
    if (obj.type() == SolarObject::Planet)
        return true;
    if (obj.type() == SolarObject::Sun)
        return true;
    return obj.archetype().contains(QStringLiteral("planet"), Qt::CaseInsensitive);
}

SolarObject *SystemEditorPage::findRingHostForSelection() const
{
    if (!m_document || m_selectedNicknames.size() != 1)
        return nullptr;

    const QString selectedNickname = m_selectedNicknames.first();
    if (SolarObject *selectedObject = findObjectByNickname(selectedNickname)) {
        if (RingEditService::canHostRing(*selectedObject))
            return selectedObject;
    }

    if (ZoneItem *selectedZone = findZoneByNickname(selectedNickname))
        return RingEditService::findHostForZone(m_document.get(), selectedZone->nickname());

    return nullptr;
}

SolarObject *SystemEditorPage::findRingHostAtScenePos(const QPointF &scenePos) const
{
    if (!m_mapScene)
        return nullptr;

    const auto sceneItems = m_mapScene->items(scenePos);
    for (QGraphicsItem *item : sceneItems) {
        auto *solarItem = dynamic_cast<flatlas::rendering::SolarObjectItem *>(item);
        if (!solarItem)
            continue;
        SolarObject *hostObject = findObjectByNickname(solarItem->nickname());
        if (hostObject && RingEditService::canHostRing(*hostObject))
            return hostObject;
    }
    return nullptr;
}

bool SystemEditorPage::openRingDialogForHost(SolarObject *hostObject, bool forceEnableForCreate)
{
    if (!m_document || !hostObject)
        return false;

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const RingEditOptions options = RingEditService::loadOptions(gameRoot);
    RingEditState initialState = RingEditService::loadState(m_document.get(), *hostObject, gameRoot);
    if (forceEnableForCreate && !RingEditService::hasRing(*hostObject))
        initialState.enabled = true;

    const bool showHostRadiusSphere = hostObject->type() == SolarObject::Planet || hostObject->type() == SolarObject::Sun;
    const bool hostRadiusSphereIsSun = hostObject->type() == SolarObject::Sun;
    const double hostRadius = showHostRadiusSphere
        ? static_cast<double>(RingEditService::resolvedHostRadius(*hostObject, gameRoot))
        : 0.0;

    ObjectRingDialog dialog(hostObject->nickname(),
                            hostObject->archetype(),
                            showHostRadiusSphere,
                            hostRadius,
                            hostRadiusSphereIsSun,
                            options,
                            initialState,
                            this);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const RingEditRequest request = dialog.result();
    RingEditState newState;
    newState.enabled = request.enabled;
    newState.ringIni = request.ringIni;
    newState.zoneNickname = request.zoneNickname;
    newState.outerRadius = request.outerRadius;
    newState.innerRadius = request.innerRadius;
    newState.thickness = request.thickness;
    newState.rotateX = request.rotateX;
    newState.rotateY = request.rotateY;
    newState.rotateZ = request.rotateZ;
    newState.sortValue = initialState.sortValue;

    QString errorMessage;
    if (!RingEditService::apply(m_document.get(), hostObject, newState, &errorMessage)) {
        QMessageBox::warning(this,
                             newState.enabled ? tr("Ring erstellen") : tr("Ring entfernen"),
                             errorMessage.trimmed().isEmpty() ? tr("Der Ring konnte nicht gespeichert werden.") : errorMessage);
        return false;
    }

    m_document->setDirty(true);
    refreshObjectList();
    m_selectedNicknames = {hostObject->nickname()};
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
    return true;
}

bool SystemEditorPage::isChildObject(const SolarObject &obj) const
{
    return !parentNicknameForObject(obj).isEmpty();
}

bool SystemEditorPage::hasSingleObjectGroupSelection() const
{
    return m_selectedNicknames.size() == 1 && findObjectByNickname(m_selectedNicknames.first()) != nullptr;
}

void SystemEditorPage::open3DPreviewForSelection()
{
    if (!hasSingleObjectGroupSelection())
        return;

    SolarObject *rootObject = findObjectByNickname(m_selectedNicknames.first());
    if (!rootObject)
        return;

    const QStringList groupNicknames = objectGroupNicknames(rootObject->nickname());
    const auto &modelPaths = solarArchetypeModelPathsForPreview();

    if (groupNicknames.size() == 1) {
        const QString modelPath = modelPaths.value(normalizedPathKey(rootObject->archetype()));
        if (modelPath.isEmpty()) {
            QMessageBox::warning(this, tr("3D Preview"),
                                 tr("Für das ausgewählte Objekt konnte kein Modell aufgelöst werden."));
            return;
        }

        auto dialog = std::make_unique<SystemGroupPreviewDialog>(this);
        QString errorMessage;
        if (!dialog->loadModelFile(modelPath, rootObject->nickname(), &errorMessage)) {
            QMessageBox::warning(this, tr("3D Preview"), errorMessage);
            return;
        }
        dialog->exec();
        return;
    }

    flatlas::infrastructure::ModelNode combinedRoot;
    combinedRoot.name = rootObject->nickname();
    const QVector3D rootPosition = rootObject->position();
    int addedModels = 0;

    for (const QString &groupNickname : groupNicknames) {
        SolarObject *object = findObjectByNickname(groupNickname);
        if (!object)
            continue;

        const QString modelPath = modelPaths.value(normalizedPathKey(object->archetype()));
        if (modelPath.isEmpty())
            continue;

        const flatlas::infrastructure::DecodedModel decoded = flatlas::rendering::ModelCache::instance().load(modelPath);
        if (!decoded.isValid())
            continue;

        flatlas::infrastructure::ModelNode wrapper;
        wrapper.name = object->nickname();
        wrapper.origin = object->position() - rootPosition;
        wrapper.rotation = quaternionFromFreelancerRotation(object->rotation());
        wrapper.children.append(decoded.rootNode);
        combinedRoot.children.append(wrapper);
        ++addedModels;
    }

    if (addedModels <= 0) {
        QMessageBox::warning(this, tr("3D Preview"),
                             tr("Für die ausgewählte Objektgruppe konnte kein darstellbares Modell geladen werden."));
        return;
    }

    auto dialog = std::make_unique<SystemGroupPreviewDialog>(this);
    QString errorMessage;
    if (!dialog->loadModelNode(combinedRoot,
                               tr("%1 (%2 parts)").arg(rootObject->nickname()).arg(groupNicknames.size()),
                               &errorMessage)) {
        QMessageBox::warning(this, tr("3D Preview"), errorMessage);
        return;
    }
    dialog->exec();
}

} // namespace flatlas::editors
