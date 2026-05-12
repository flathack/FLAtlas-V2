// tools/PathFinderDialog.cpp – Dialog zur Pfadfindung zwischen Systemen

#include "PathFinderDialog.h"
#include "ShortestPathGenerator.h"
#include "core/PathUtils.h"
#include "domain/UniverseData.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>

namespace flatlas::tools {

namespace {

constexpr double kDefaultCruiseSpeed = 300.0;
constexpr double kTradeLaneSpeed = 2500.0;
constexpr int kGateTimeSeconds = 10;
constexpr double kFallbackLegDistance = 35000.0;

struct TravelJump {
    QString nickname;
    QString system;
    QString targetSystem;
    QString targetObject;
    QString label;
    QVector3D position;
};

struct TravelLane {
    QString system;
    QStringList labels;
    QVector<QVector3D> positions;
};

struct TravelObject {
    QString nickname;
    QString label;
    QString system;
    QVector3D position;
};

struct TravelData {
    double cruiseSpeed = kDefaultCruiseSpeed;
    QVector<TravelJump> jumps;
    QVector<TravelLane> lanes;
    QHash<QString, QVector<TravelObject>> objectsBySystem;
};

struct TravelBreakdown {
    QStringList lines;
    QVector<QStringList> rows;
    double seconds = 0.0;
};

QString normalized(QString value)
{
    return value.trimmed().toLower();
}

QVector3D parsePos(const QString &value)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (flatlas::core::PathUtils::parsePosition(value, x, y, z))
        return {x, y, z};
    const QStringList parts = value.split(QLatin1Char(','));
    if (parts.size() >= 2)
        return {parts.at(0).trimmed().toFloat(), 0.0f, parts.at(1).trimmed().toFloat()};
    return {};
}

bool hasPosition(const QVector3D &position)
{
    return !qFuzzyIsNull(position.x()) || !qFuzzyIsNull(position.y()) || !qFuzzyIsNull(position.z());
}

double distanceMeters(const QVector3D &from, const QVector3D &to)
{
    if (!hasPosition(from) || !hasPosition(to))
        return kFallbackLegDistance;
    const double dx = static_cast<double>(to.x() - from.x());
    const double dz = static_cast<double>(to.z() - from.z());
    return std::hypot(dx, dz);
}

QString dataPathForUniverseFile(const QString &universeFilePath)
{
    if (universeFilePath.trimmed().isEmpty())
        return {};
    const QString universeDir = QFileInfo(universeFilePath).absolutePath();
    return QFileInfo(universeDir).absolutePath();
}

QString freelancerExeDirForDataPath(const QString &dataPath)
{
    const QFileInfo dataInfo(dataPath);
    const QString gameRoot = dataInfo.fileName().compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0
        ? dataInfo.absolutePath()
        : dataPath;
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    return exeDir.isEmpty() ? gameRoot : exeDir;
}

QString resolvedIdsDisplayName(const flatlas::infrastructure::IdsStringTable &ids, int idsName, const QString &fallback)
{
    const QString displayName = idsName > 0 ? ids.getString(idsName).trimmed() : QString();
    return displayName.isEmpty() ? fallback.trimmed() : displayName;
}

QString systemFileAbsolutePath(const QString &universeFilePath, const flatlas::domain::SystemInfo &system)
{
    const QString universeDir = QFileInfo(universeFilePath).absolutePath();
    QString absolute = flatlas::core::PathUtils::ciResolvePath(universeDir, system.filePath);
    if (!absolute.isEmpty())
        return absolute;
    const QString dataDir = QFileInfo(universeDir).absolutePath();
    absolute = flatlas::core::PathUtils::ciResolvePath(dataDir, system.filePath);
    if (!absolute.isEmpty())
        return absolute;
    return QDir(universeDir).filePath(system.filePath);
}

double loadCruiseSpeed(const QString &dataPath)
{
    const QString constantsPath = flatlas::core::PathUtils::ciResolvePath(dataPath, QStringLiteral("constants.ini"));
    if (constantsPath.isEmpty() || !QFile::exists(constantsPath))
        return kDefaultCruiseSpeed;
    const auto doc = flatlas::infrastructure::IniParser::parseFile(constantsPath);
    const QStringList keys{QStringLiteral("CRUISE_SPEED"), QStringLiteral("cruise_speed"), QStringLiteral("CruiseSpeed")};
    for (const auto &section : doc) {
        for (const QString &key : keys) {
            bool ok = false;
            const double value = section.value(key).trimmed().toDouble(&ok);
            if (ok && value > 0.0)
                return value;
        }
    }
    return kDefaultCruiseSpeed;
}

TravelData scanTravelData(const flatlas::domain::UniverseData *universe, const QString &universeFilePath)
{
    TravelData data;
    const QString dataPath = dataPathForUniverseFile(universeFilePath);
    flatlas::infrastructure::IdsStringTable ids;
    if (!dataPath.isEmpty()) {
        data.cruiseSpeed = loadCruiseSpeed(dataPath);
        ids.loadFromFreelancerDir(freelancerExeDirForDataPath(dataPath));
    }
    if (!universe || universeFilePath.trimmed().isEmpty())
        return data;

    struct Ring {
        QString nickname;
        QString previous;
        QString next;
        QVector3D position;
    };

    for (const auto &system : universe->systems) {
        const QString filePath = systemFileAbsolutePath(universeFilePath, system);
        if (!QFile::exists(filePath))
            continue;
        const auto doc = flatlas::infrastructure::IniParser::parseFile(filePath);
        QHash<QString, Ring> rings;
        for (const auto &section : doc) {
            if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString archetype = section.value(QStringLiteral("archetype")).trimmed();
            const QString previous = section.value(QStringLiteral("prev_ring")).trimmed();
            const QString next = section.value(QStringLiteral("next_ring")).trimmed();
            const QVector3D position = parsePos(section.value(QStringLiteral("pos")));
            const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt();
            const QString displayName = resolvedIdsDisplayName(ids, idsName, nickname);
            if (!nickname.isEmpty()) {
                TravelObject object;
                object.nickname = nickname;
                object.label = displayName;
                object.system = system.nickname;
                object.position = position;
                data.objectsBySystem[normalized(system.nickname)].append(object);
            }
            if (!nickname.isEmpty()
                && (archetype.contains(QStringLiteral("trade_lane_ring"), Qt::CaseInsensitive)
                    || !previous.isEmpty()
                    || !next.isEmpty())) {
                rings.insert(normalized(nickname), {nickname, previous, next, position});
            }

            const QString gotoValue = section.value(QStringLiteral("goto")).trimmed();
            if (gotoValue.isEmpty())
                continue;
            const QStringList parts = gotoValue.split(QLatin1Char(','));
            if (parts.isEmpty())
                continue;
            TravelJump jump;
            jump.nickname = nickname;
            jump.system = system.nickname;
            jump.targetSystem = parts.at(0).trimmed();
            if (parts.size() > 1)
                jump.targetObject = parts.at(1).trimmed();
            jump.label = displayName;
            jump.position = position;
            if (!jump.targetSystem.isEmpty())
                data.jumps.append(jump);
        }

        QSet<QString> consumed;
        for (auto it = rings.constBegin(); it != rings.constEnd(); ++it) {
            QString start = it.key();
            QSet<QString> walkedPrevious;
            while (rings.contains(start)) {
                const QString previous = normalized(rings.value(start).previous);
                if (previous.isEmpty() || walkedPrevious.contains(previous) || !rings.contains(previous))
                    break;
                walkedPrevious.insert(previous);
                start = previous;
            }
            if (consumed.contains(start))
                continue;
            TravelLane lane;
            lane.system = system.nickname;
            QString current = start;
            QSet<QString> walkedNext;
            while (rings.contains(current) && !walkedNext.contains(current)) {
                walkedNext.insert(current);
                consumed.insert(current);
                const Ring ring = rings.value(current);
                lane.labels.append(ring.nickname);
                lane.positions.append(ring.position);
                current = normalized(ring.next);
                if (current.isEmpty())
                    break;
            }
            if (lane.positions.size() >= 2)
                data.lanes.append(lane);
        }
    }
    return data;
}

QString jumpLabel(const TravelJump &jump)
{
    return jump.label.trimmed().isEmpty() ? jump.nickname : jump.label;
}

QString objectLabel(const TravelObject &object)
{
    return object.label.trimmed().isEmpty() ? object.nickname : object.label;
}

QString objectComboLabel(const TravelObject &object)
{
    const QString label = objectLabel(object);
    return label.compare(object.nickname, Qt::CaseInsensitive) == 0
        ? object.nickname
        : QStringLiteral("%1 (%2)").arg(label, object.nickname);
}

QString extractNicknameFromComboText(const QString &text)
{
    const int parenStart = text.lastIndexOf(QLatin1Char('('));
    const int parenEnd = text.lastIndexOf(QLatin1Char(')'));
    if (parenStart >= 0 && parenEnd > parenStart)
        return text.mid(parenStart + 1, parenEnd - parenStart - 1).trimmed();
    return text.trimmed();
}

QString formatSeconds(int seconds)
{
    const int minutes = seconds / 60;
    const int remainingSeconds = seconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}

TravelBreakdown surfaceBreakdown(const TravelData &data,
                                 const QString &system,
                                 const QString &fromLabel,
                                 const QVector3D &fromPos,
                                 const QString &toLabel,
                                 const QVector3D &toPos)
{
    TravelBreakdown best;
    const double directDistance = distanceMeters(fromPos, toPos);
    best.seconds = directDistance / qMax(1.0, data.cruiseSpeed);
    best.lines.append(QObject::tr("Cruise in %1: %2 -> %3 (%4 s)")
                          .arg(system, fromLabel, toLabel)
                          .arg(qRound(best.seconds)));
    best.rows.append({QObject::tr("Cruise"), system, fromLabel, toLabel, formatSeconds(qRound(best.seconds))});

    if (!hasPosition(fromPos) || !hasPosition(toPos))
        return best;

    for (const auto &lane : data.lanes) {
        if (lane.system.compare(system, Qt::CaseInsensitive) != 0 || lane.positions.size() < 2)
            continue;
        int fromIndex = 0;
        int toIndex = 0;
        double fromDistance = std::numeric_limits<double>::max();
        double toDistance = std::numeric_limits<double>::max();
        for (int i = 0; i < lane.positions.size(); ++i) {
            const double dFrom = distanceMeters(fromPos, lane.positions.at(i));
            const double dTo = distanceMeters(toPos, lane.positions.at(i));
            if (dFrom < fromDistance) {
                fromDistance = dFrom;
                fromIndex = i;
            }
            if (dTo < toDistance) {
                toDistance = dTo;
                toIndex = i;
            }
        }
        double laneDistance = 0.0;
        for (int i = qMin(fromIndex, toIndex); i < qMax(fromIndex, toIndex); ++i)
            laneDistance += distanceMeters(lane.positions.at(i), lane.positions.at(i + 1));
        const double seconds = (fromDistance / qMax(1.0, data.cruiseSpeed))
            + (laneDistance / kTradeLaneSpeed)
            + (toDistance / qMax(1.0, data.cruiseSpeed));
        if (seconds < best.seconds) {
            best.seconds = seconds;
            best.lines = {QObject::tr("Trade lane in %1: %2 -> %3 (%4 s)")
                              .arg(system, lane.labels.value(fromIndex), lane.labels.value(toIndex))
                              .arg(qRound(seconds))};
            best.rows = {{QObject::tr("Trade Lane"), system, lane.labels.value(fromIndex), lane.labels.value(toIndex), formatSeconds(qRound(seconds))}};
        }
    }
    return best;
}

const TravelJump *findJump(const TravelData &data, const QString &system, const QString &targetSystem)
{
    for (const auto &jump : data.jumps) {
        if (jump.system.compare(system, Qt::CaseInsensitive) == 0
            && jump.targetSystem.compare(targetSystem, Qt::CaseInsensitive) == 0
            && jump.targetSystem.compare(system, Qt::CaseInsensitive) != 0) {
            return &jump;
        }
    }
    return nullptr;
}

TravelBreakdown intraSystemBreakdown(const TravelData &data,
                                     const QString &system,
                                     const QString &fromLabel,
                                     const QVector3D &fromPos,
                                     const QString &toLabel,
                                     const QVector3D &toPos)
{
    QVector<TravelJump> localJumps;
    QHash<QString, TravelJump> jumpByNick;
    for (const auto &jump : data.jumps) {
        if (!jump.nickname.isEmpty())
            jumpByNick.insert(normalized(jump.nickname), jump);
        if (jump.system.compare(system, Qt::CaseInsensitive) == 0
            && jump.targetSystem.compare(system, Qt::CaseInsensitive) == 0
            && !jump.targetObject.isEmpty()) {
            localJumps.append(jump);
        }
    }
    if (localJumps.isEmpty())
        return surfaceBreakdown(data, system, fromLabel, fromPos, toLabel, toPos);

    struct Node { QString id; QString label; QVector3D pos; };
    QVector<Node> nodes{{QStringLiteral("start"), fromLabel, fromPos}, {QStringLiteral("end"), toLabel, toPos}};
    for (const auto &jump : localJumps)
        nodes.append({QStringLiteral("jump:%1").arg(jump.nickname), jumpLabel(jump), jump.position});

    QHash<QString, Node> nodeById;
    for (const auto &node : nodes)
        nodeById.insert(node.id, node);
    QHash<QString, double> dist;
    QHash<QString, QPair<QString, TravelBreakdown>> prev;
    QSet<QString> visited;
    for (const auto &node : nodes)
        dist.insert(node.id, std::numeric_limits<double>::max());
    dist[QStringLiteral("start")] = 0.0;

    while (visited.size() < nodes.size()) {
        QString current;
        double best = std::numeric_limits<double>::max();
        for (const auto &node : nodes) {
            if (!visited.contains(node.id) && dist.value(node.id) < best) {
                best = dist.value(node.id);
                current = node.id;
            }
        }
        if (current.isEmpty() || current == QStringLiteral("end") || !std::isfinite(best))
            break;
        visited.insert(current);

        for (const auto &node : nodes) {
            if (node.id == current || visited.contains(node.id))
                continue;
            const auto move = surfaceBreakdown(data, system, nodeById.value(current).label, nodeById.value(current).pos, node.label, node.pos);
            const double next = best + move.seconds;
            if (next < dist.value(node.id)) {
                dist[node.id] = next;
                prev[node.id] = {current, move};
            }
        }

        if (!current.startsWith(QStringLiteral("jump:")))
            continue;
        const QString jumpNick = current.mid(QStringLiteral("jump:").size());
        const TravelJump jump = jumpByNick.value(normalized(jumpNick));
        const QString targetId = QStringLiteral("jump:%1").arg(jump.targetObject);
        if (!nodeById.contains(targetId) || visited.contains(targetId))
            continue;
        const double next = best + kGateTimeSeconds;
        if (next < dist.value(targetId)) {
            TravelBreakdown jumpBreakdown;
            jumpBreakdown.seconds = kGateTimeSeconds;
            jumpBreakdown.lines = {QObject::tr("Local jump in %1: %2 -> %3 (%4 s)")
                                       .arg(system, jumpLabel(jump), nodeById.value(targetId).label)
                                       .arg(kGateTimeSeconds)};
            jumpBreakdown.rows = {{QObject::tr("Local Jump"), system, jumpLabel(jump), nodeById.value(targetId).label, formatSeconds(kGateTimeSeconds)}};
            dist[targetId] = next;
            prev[targetId] = {current, jumpBreakdown};
        }
    }

    if (!std::isfinite(dist.value(QStringLiteral("end"))))
        return surfaceBreakdown(data, system, fromLabel, fromPos, toLabel, toPos);

    TravelBreakdown result;
    result.seconds = dist.value(QStringLiteral("end"));
    QString cursor = QStringLiteral("end");
    while (cursor != QStringLiteral("start") && prev.contains(cursor)) {
        const auto step = prev.value(cursor);
        result.lines = step.second.lines + result.lines;
        result.rows = step.second.rows + result.rows;
        cursor = step.first;
    }
    return result;
}

TravelBreakdown pathTravelBreakdown(const TravelData &data,
                                    const QStringList &path,
                                    const TravelObject *startObject,
                                    const TravelObject *targetObject)
{
    TravelBreakdown result;
    if (path.size() == 1 && startObject && targetObject) {
        return intraSystemBreakdown(data,
                                    path.first(),
                                    objectLabel(*startObject),
                                    startObject->position,
                                    objectLabel(*targetObject),
                                    targetObject->position);
    }
    if (path.size() < 2)
        return result;

    for (int i = 0; i < path.size() - 1; ++i) {
        const QString currentSystem = path.at(i);
        const QString nextSystem = path.at(i + 1);
        const TravelJump *departure = findJump(data, currentSystem, nextSystem);
        const TravelJump *arrival = findJump(data, nextSystem, currentSystem);
        if (!departure || !arrival) {
            result.lines.append(QObject::tr("Missing jump object data for %1 -> %2.").arg(currentSystem, nextSystem));
            result.rows.append({QObject::tr("Missing Data"), currentSystem, currentSystem, nextSystem, formatSeconds(kGateTimeSeconds)});
            result.seconds += kGateTimeSeconds;
            continue;
        }

        if (i == 0 && startObject) {
            const auto intra = intraSystemBreakdown(data,
                                                    currentSystem,
                                                    objectLabel(*startObject),
                                                    startObject->position,
                                                    jumpLabel(*departure),
                                                    departure->position);
            result.seconds += intra.seconds;
            result.lines += intra.lines;
            result.rows += intra.rows;
        } else if (i > 0) {
            const TravelJump *previousArrival = findJump(data, currentSystem, path.at(i - 1));
            if (previousArrival) {
                const auto intra = intraSystemBreakdown(data,
                                                        currentSystem,
                                                        jumpLabel(*previousArrival),
                                                        previousArrival->position,
                                                        jumpLabel(*departure),
                                                        departure->position);
                result.seconds += intra.seconds;
                result.lines += intra.lines;
                result.rows += intra.rows;
            }
        }

        result.seconds += kGateTimeSeconds;
        result.lines.append(QObject::tr("Jump %1 -> %2 (%3 s)").arg(currentSystem, nextSystem).arg(kGateTimeSeconds));
        result.rows.append({QObject::tr("Jump"), QStringLiteral("%1 -> %2").arg(currentSystem, nextSystem), jumpLabel(*departure), jumpLabel(*arrival), formatSeconds(kGateTimeSeconds)});

        if (i == path.size() - 2 && targetObject) {
            const auto intra = intraSystemBreakdown(data,
                                                    nextSystem,
                                                    jumpLabel(*arrival),
                                                    arrival->position,
                                                    objectLabel(*targetObject),
                                                    targetObject->position);
            result.seconds += intra.seconds;
            result.lines += intra.lines;
            result.rows += intra.rows;
        }
    }
    return result;
}

bool findObject(const TravelData &data, const QString &system, const QString &nickname, TravelObject *outObject)
{
    const auto objects = data.objectsBySystem.value(normalized(system));
    for (const auto &object : objects) {
        if (object.nickname.compare(nickname, Qt::CaseInsensitive) == 0) {
            if (outObject)
                *outObject = object;
            return true;
        }
    }
    return false;
}

} // namespace

PathFinderDialog::PathFinderDialog(const flatlas::domain::UniverseData *universe,
                                   const QString &universeFilePath,
                                   QWidget *parent)
    : QDialog(parent), m_universe(universe), m_universeFilePath(universeFilePath)
{
    setWindowTitle(tr("Shortest Path Finder"));
    setMinimumSize(450, 350);
    resize(500, 400);
    buildUi();
    populateSystems();
}

void PathFinderDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Input area
    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("From System:")), 0, 0);
    m_fromCombo = new QComboBox;
    m_fromCombo->setEditable(true);
    m_fromCombo->setInsertPolicy(QComboBox::NoInsert);
    grid->addWidget(m_fromCombo, 0, 1);

    grid->addWidget(new QLabel(tr("From Object:")), 1, 0);
    m_fromObjectCombo = new QComboBox;
    m_fromObjectCombo->setEditable(true);
    m_fromObjectCombo->setInsertPolicy(QComboBox::NoInsert);
    grid->addWidget(m_fromObjectCombo, 1, 1);

    grid->addWidget(new QLabel(tr("To System:")), 2, 0);
    m_toCombo = new QComboBox;
    m_toCombo->setEditable(true);
    m_toCombo->setInsertPolicy(QComboBox::NoInsert);
    grid->addWidget(m_toCombo, 2, 1);

    grid->addWidget(new QLabel(tr("To Object:")), 3, 0);
    m_toObjectCombo = new QComboBox;
    m_toObjectCombo->setEditable(true);
    m_toObjectCombo->setInsertPolicy(QComboBox::NoInsert);
    grid->addWidget(m_toObjectCombo, 3, 1);

    m_findButton = new QPushButton(tr("Find Shortest Path"));
    grid->addWidget(m_findButton, 4, 0, 1, 2);

    mainLayout->addLayout(grid);

    // Result area
    m_resultLabel = new QLabel;
    m_resultLabel->setWordWrap(true);
    mainLayout->addWidget(m_resultLabel);

    m_pathDisplay = new QTextEdit;
    m_pathDisplay->setReadOnly(true);
    m_pathDisplay->setMaximumHeight(140);
    mainLayout->addWidget(m_pathDisplay);

    m_segmentTable = new QTableWidget;
    m_segmentTable->setColumnCount(5);
    m_segmentTable->setHorizontalHeaderLabels({tr("Type"), tr("System"), tr("From"), tr("To"), tr("Time")});
    m_segmentTable->horizontalHeader()->setStretchLastSection(true);
    m_segmentTable->verticalHeader()->setVisible(false);
    m_segmentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_segmentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_segmentTable->setAlternatingRowColors(true);
    mainLayout->addWidget(m_segmentTable);

    connect(m_fromCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        populateObjectCombo(m_fromObjectCombo, extractNicknameFromComboText(text));
    });
    connect(m_toCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        populateObjectCombo(m_toObjectCombo, extractNicknameFromComboText(text));
    });
    connect(m_findButton, &QPushButton::clicked, this, &PathFinderDialog::onFindPath);
}

void PathFinderDialog::populateSystems()
{
    if (!m_universe) return;

    QStringList names;
    for (const auto &sys : m_universe->systems) {
        const QString label = sys.displayName.isEmpty()
            ? sys.nickname
            : QStringLiteral("%1 (%2)").arg(sys.displayName, sys.nickname);
        names.append(label);
    }
    names.sort();

    m_fromCombo->addItems(names);
    m_toCombo->addItems(names);

    if (m_toCombo->count() > 1)
        m_toCombo->setCurrentIndex(1);
    if (m_fromCombo->count() > 0)
        populateObjectCombo(m_fromObjectCombo, extractNicknameFromComboText(m_fromCombo->currentText()));
    if (m_toCombo->count() > 0)
        populateObjectCombo(m_toObjectCombo, extractNicknameFromComboText(m_toCombo->currentText()));
}

void PathFinderDialog::populateObjectCombo(QComboBox *combo, const QString &systemNickname)
{
    if (!combo)
        return;
    combo->blockSignals(true);
    combo->clear();

    const TravelData data = scanTravelData(m_universe, m_universeFilePath);
    QVector<TravelObject> objects = data.objectsBySystem.value(normalized(systemNickname));
    std::sort(objects.begin(), objects.end(), [](const TravelObject &left, const TravelObject &right) {
        return objectComboLabel(left).toLower() < objectComboLabel(right).toLower();
    });
    for (const auto &object : objects)
        combo->addItem(objectComboLabel(object), object.nickname);
    combo->blockSignals(false);
}

void PathFinderDialog::onFindPath()
{
    if (!m_universe) return;

    const QString from = extractNicknameFromComboText(m_fromCombo->currentText());
    const QString to = extractNicknameFromComboText(m_toCombo->currentText());
    const QString fromObjectNickname = m_fromObjectCombo ? m_fromObjectCombo->currentData().toString() : QString();
    const QString toObjectNickname = m_toObjectCombo ? m_toObjectCombo->currentData().toString() : QString();

    ShortestPathGenerator pathGen(m_universe);
    QStringList path = pathGen.findShortestPath(from, to);

    m_lastPath = path;

    if (path.isEmpty()) {
        m_resultLabel->setText(tr("<b>No route found</b> between %1 and %2.")
                                  .arg(from, to));
        m_pathDisplay->clear();
        if (m_segmentTable)
            m_segmentTable->setRowCount(0);
    } else {
        const TravelData travelData = scanTravelData(m_universe, m_universeFilePath);
        TravelObject fromObject;
        TravelObject toObject;
        const bool hasFromObject = findObject(travelData, from, fromObjectNickname, &fromObject);
        const bool hasToObject = findObject(travelData, to, toObjectNickname, &toObject);
        const TravelBreakdown travel = pathTravelBreakdown(travelData,
                                                           path,
                                                           hasFromObject ? &fromObject : nullptr,
                                                           hasToObject ? &toObject : nullptr);
        const int travelSeconds = qRound(travel.seconds);
        m_resultLabel->setText(tr("<b>Route found:</b> %1 jumps, distance: %2, flight time: %3")
                                  .arg(pathGen.lastPathJumps())
                                  .arg(pathGen.lastPathDistance(), 0, 'f', 1)
                                  .arg(formatSeconds(travelSeconds)));

        // Pfad anzeigen mit Systemnamen
        QStringList displayPath;
        if (hasFromObject || hasToObject) {
            displayPath.append(tr("From: %1 / %2").arg(from, hasFromObject ? objectComboLabel(fromObject) : tr("No object selected")));
            displayPath.append(tr("To: %1 / %2").arg(to, hasToObject ? objectComboLabel(toObject) : tr("No object selected")));
            displayPath.append(QString());
        }
        displayPath.append(tr("System path:"));
        for (int i = 0; i < path.size(); ++i) {
            const auto *sys = m_universe->findSystem(path[i]);
            QString name = sys && !sys->displayName.isEmpty()
                ? QStringLiteral("%1 (%2)").arg(sys->displayName, path[i])
                : path[i];
            displayPath.append(QStringLiteral("%1. %2").arg(i + 1).arg(name));
        }
        if (!travel.lines.isEmpty()) {
            displayPath.append(QString());
            displayPath.append(tr("Flight time details:"));
            displayPath += travel.lines;
        }
        m_pathDisplay->setPlainText(displayPath.join(QLatin1Char('\n')));

        if (m_segmentTable) {
            m_segmentTable->setRowCount(travel.rows.size());
            for (int row = 0; row < travel.rows.size(); ++row) {
                const QStringList cells = travel.rows.at(row);
                for (int column = 0; column < m_segmentTable->columnCount(); ++column) {
                    auto *item = new QTableWidgetItem(cells.value(column));
                    if (column == 4)
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    m_segmentTable->setItem(row, column, item);
                }
            }
            m_segmentTable->resizeColumnsToContents();
        }

        emit pathFound(path);
    }
}

} // namespace flatlas::tools
