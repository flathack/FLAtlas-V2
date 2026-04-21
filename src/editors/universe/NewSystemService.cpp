#include "NewSystemService.h"

#include "UniverseSerializer.h"
#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/freelancer/ResourceDllWriter.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QtAlgorithms>

namespace flatlas::editors {

using namespace flatlas::domain;
using namespace flatlas::infrastructure;

namespace {

QString toBackslashPath(QString path)
{
    path.replace(QLatin1Char('/'), QLatin1Char('\\'));
    return path;
}

QString normalizeOptionPath(const QString &path)
{
    return toBackslashPath(QDir::cleanPath(path.trimmed()));
}

void insertUniqueSorted(QStringList &list, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return;
    for (const QString &existing : list) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0)
            return;
    }
    list.append(trimmed);
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
}

QString universeDirFromUniverseFile(const QString &universeFilePath)
{
    return QFileInfo(universeFilePath).absolutePath();
}

QString dataDirFromUniverseFile(const QString &universeFilePath)
{
    return QFileInfo(universeDirFromUniverseFile(universeFilePath)).absolutePath();
}

QString gameDirForUniverseFile(const QString &universeFilePath)
{
    const QString ctxGamePath = flatlas::core::EditingContext::instance().primaryGamePath();
    if (!ctxGamePath.trimmed().isEmpty())
        return ctxGamePath;
    return QFileInfo(dataDirFromUniverseFile(universeFilePath)).absolutePath();
}

QString contextDataDir(const QString &universeFilePath)
{
    const QString gameDir = gameDirForUniverseFile(universeFilePath);
    if (gameDir.trimmed().isEmpty())
        return dataDirFromUniverseFile(universeFilePath);

    const QString resolved = flatlas::core::PathUtils::ciResolvePath(gameDir, QStringLiteral("DATA"));
    if (!resolved.isEmpty())
        return resolved;
    return QDir(gameDir).absoluteFilePath(QStringLiteral("DATA"));
}

QString contextFreelancerIniPath(const QString &universeFilePath)
{
    const QString gameDir = gameDirForUniverseFile(universeFilePath);
    if (gameDir.trimmed().isEmpty())
        return {};

    QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameDir, QStringLiteral("EXE"));
    if (exeDir.isEmpty())
        exeDir = QDir(gameDir).absoluteFilePath(QStringLiteral("EXE"));

    QString freelancerIni = flatlas::core::PathUtils::ciResolvePath(exeDir, QStringLiteral("freelancer.ini"));
    if (!freelancerIni.isEmpty())
        return freelancerIni;
    return QDir(exeDir).absoluteFilePath(QStringLiteral("freelancer.ini"));
}

QString systemIniText(const QString &nickname, const NewSystemRequest &request)
{
    return QStringLiteral(
        "[SystemInfo]\n"
        "space_color = %1\n"
        "local_faction = %2\n"
        "\n"
        "[TexturePanels]\n"
        "file = universe\\heavens\\shapes.ini\n"
        "\n"
        "[Music]\n"
        "space = %3\n"
        "danger = %4\n"
        "battle = %5\n"
        "\n"
        "[Dust]\n"
        "spacedust = Dust\n"
        "\n"
        "[Ambient]\n"
        "color = %6\n"
        "\n"
        "[Background]\n"
        "basic_stars = %7\n"
        "complex_stars = %8\n"
        "nebulae = %9\n"
        "\n"
        "[LightSource]\n"
        "nickname = %10_system_light\n"
        "pos = 0, 0, 0\n"
        "color = %11\n"
        "range = %12\n"
        "type = DIRECTIONAL\n"
        "atten_curve = DYNAMIC_DIRECTION\n"
        "\n")
        .arg(request.spaceColor,
             request.localFactionNickname,
             request.musicSpace,
             request.musicDanger,
             request.musicBattle,
             request.ambientColor,
             request.basicStars,
             request.complexStars,
             request.nebulae,
             nickname,
             request.lightSourceColor)
        .arg(request.systemSize);
}

QString resolvedIdsName(const IdsStringTable &ids, int id)
{
    if (id <= 0)
        return {};
    return ids.getString(id).trimmed();
}

} // namespace

NewSystemDialogOptions NewSystemService::scanOptions(const QString &universeFilePath,
                                                     const UniverseData *data)
{
    Q_UNUSED(data);
    NewSystemDialogOptions options;

    insertUniqueSorted(options.musicSpaceOptions, QStringLiteral("music_br_space"));
    insertUniqueSorted(options.musicDangerOptions, QStringLiteral("music_br_danger"));
    insertUniqueSorted(options.musicBattleOptions, QStringLiteral("music_br_battle"));
    insertUniqueSorted(options.basicStarsOptions, QStringLiteral("solar\\starsphere\\starsphere_stars_basic.cmp"));
    insertUniqueSorted(options.complexStarsOptions, QStringLiteral("solar\\starsphere\\starsphere_br01_stars.cmp"));
    insertUniqueSorted(options.nebulaeOptions, QStringLiteral("solar\\starsphere\\starsphere_br01.cmp"));

    const QString dataDir = contextDataDir(universeFilePath);
    const QString systemsDir =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/SYSTEMS"));
    if (!systemsDir.isEmpty()) {
        QDirIterator it(systemsDir, QStringList() << QStringLiteral("*.ini"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString systemIniPath = it.next();
            const IniDocument doc = IniParser::parseFile(systemIniPath);
            for (const IniSection &section : doc) {
                if (section.name.compare(QStringLiteral("Music"), Qt::CaseInsensitive) == 0) {
                    insertUniqueSorted(options.musicSpaceOptions, section.value(QStringLiteral("space")));
                    insertUniqueSorted(options.musicDangerOptions, section.value(QStringLiteral("danger")));
                    insertUniqueSorted(options.musicBattleOptions, section.value(QStringLiteral("battle")));
                } else if (section.name.compare(QStringLiteral("Background"), Qt::CaseInsensitive) == 0) {
                    insertUniqueSorted(options.basicStarsOptions, normalizeOptionPath(section.value(QStringLiteral("basic_stars"))));
                    insertUniqueSorted(options.complexStarsOptions, normalizeOptionPath(section.value(QStringLiteral("complex_stars"))));
                    insertUniqueSorted(options.nebulaeOptions, normalizeOptionPath(section.value(QStringLiteral("nebulae"))));
                }
            }
        }
    }

    const QString starsphereDir =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/STARSPHERE"));
    if (!starsphereDir.isEmpty()) {
        QDirIterator it(starsphereDir, QStringList() << QStringLiteral("*.cmp"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            QString relative = QDir(dataDir).relativeFilePath(filePath);
            relative = normalizeOptionPath(relative);
            insertUniqueSorted(options.basicStarsOptions, relative);
            insertUniqueSorted(options.complexStarsOptions, relative);
            insertUniqueSorted(options.nebulaeOptions, relative);
        }
    }

    const QString freelancerIniPath = contextFreelancerIniPath(universeFilePath);
    const QString exeDir = QFileInfo(freelancerIniPath).absolutePath();
    IdsStringTable ids;
    if (!freelancerIniPath.isEmpty() && !exeDir.isEmpty())
        ids.loadFromFreelancerDir(exeDir);

    const QString initialworldPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("initialworld.ini"));
    if (!initialworldPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(initialworldPath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Group"), Qt::CaseInsensitive) != 0)
                continue;

            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;

            const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt();
            const QString ingameName = resolvedIdsName(ids, idsName);

            NewSystemFactionOption option;
            option.nickname = nickname;
            option.displayText = ingameName.isEmpty()
                ? nickname
                : QStringLiteral("%1 - %2").arg(nickname, ingameName);

            bool exists = false;
            for (const auto &existing : options.factionOptions) {
                if (existing.nickname.compare(option.nickname, Qt::CaseInsensitive) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                options.factionOptions.append(option);
        }
    }

    std::sort(options.factionOptions.begin(), options.factionOptions.end(),
              [](const NewSystemFactionOption &a, const NewSystemFactionOption &b) {
        return a.displayText.compare(b.displayText, Qt::CaseInsensitive) < 0;
    });

    bool hasDefaultFaction = false;
    for (const auto &option : options.factionOptions) {
        if (option.nickname.compare(QStringLiteral("li_n_grp"), Qt::CaseInsensitive) == 0) {
            hasDefaultFaction = true;
            break;
        }
    }
    if (!hasDefaultFaction)
        options.factionOptions.prepend({QStringLiteral("li_n_grp"), QStringLiteral("li_n_grp")});

    return options;
}

QString NewSystemService::nextSystemNickname(const QString &prefix, const UniverseData &data)
{
    const QString normalizedPrefix = prefix.trimmed().toUpper();
    if (normalizedPrefix.isEmpty())
        return {};

    int highest = 0;
    int width = 2;
    const QRegularExpression re(QStringLiteral("^%1(\\d+)$")
                                    .arg(QRegularExpression::escape(normalizedPrefix)),
                                QRegularExpression::CaseInsensitiveOption);

    for (const auto &system : data.systems) {
        const auto match = re.match(system.nickname.trimmed());
        if (!match.hasMatch())
            continue;
        const QString digits = match.captured(1);
        highest = std::max(highest, digits.toInt());
        width = std::max(width, static_cast<int>(digits.size()));
    }

    return QStringLiteral("%1%2")
        .arg(normalizedPrefix,
             QString::number(highest + 1).rightJustified(width, QLatin1Char('0')));
}

bool NewSystemService::createSystem(const QString &universeFilePath,
                                    const UniverseData &currentUniverse,
                                    const NewSystemRequest &request,
                                    const QPointF &position,
                                    CreatedSystemResult *result,
                                    QString *errorMessage)
{
    if (request.systemName.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Bitte einen Systemnamen eingeben.");
        return false;
    }
    if (request.systemPrefix.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Bitte einen System Prefix eingeben.");
        return false;
    }

    UniverseData updatedUniverse = currentUniverse;
    const QString nickname = nextSystemNickname(request.systemPrefix, updatedUniverse);
    if (nickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Es konnte kein gültiger System-Nickname erzeugt werden.");
        return false;
    }
    if (updatedUniverse.findSystem(nickname)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das System '%1' existiert bereits.").arg(nickname);
        return false;
    }

    const QString universeDir = universeDirFromUniverseFile(universeFilePath);
    const QString systemRelativePath =
        QStringLiteral("systems\\%1\\%1.ini").arg(nickname);
    const QString absoluteSystemDir = QDir(universeDir).absoluteFilePath(QStringLiteral("systems/%1").arg(nickname));
    const QString absoluteSystemFilePath = QDir(absoluteSystemDir).absoluteFilePath(QStringLiteral("%1.ini").arg(nickname));

    if (!QDir().mkpath(absoluteSystemDir)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der Systemordner konnte nicht erstellt werden:\n%1").arg(absoluteSystemDir);
        return false;
    }

    QFile systemFile(absoluteSystemFilePath);
    if (!systemFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Die Systemdatei konnte nicht geschrieben werden:\n%1").arg(absoluteSystemFilePath);
        return false;
    }
    systemFile.write(systemIniText(nickname, request).toUtf8());
    systemFile.close();

    const QString freelancerIniPath = contextFreelancerIniPath(universeFilePath);
    int stridName = 0;
    if (!ResourceDllWriter::ensureStringResource(freelancerIniPath,
                                                 QString(),
                                                 0,
                                                 request.systemName.trimmed(),
                                                 &stridName,
                                                 errorMessage)) {
        QFile::remove(absoluteSystemFilePath);
        QDir().rmdir(absoluteSystemDir);
        return false;
    }

    SystemInfo info;
    info.nickname = nickname;
    info.displayName = request.systemName.trimmed();
    info.filePath = systemRelativePath;
    info.position = QVector3D(static_cast<float>(position.x()), 0.0f, static_cast<float>(position.y()));
    info.idsName = 0;
    info.stridName = stridName;
    info.rawEntries = {
        {QStringLiteral("nickname"), info.nickname},
        {QStringLiteral("file"), info.filePath},
        {QStringLiteral("pos"), QStringLiteral("%1, %2").arg(position.x(), 0, 'f', 0).arg(position.y(), 0, 'f', 0)},
        {QStringLiteral("visit"), QStringLiteral("0")},
        {QStringLiteral("strid_name"), QString::number(info.stridName)},
        {QStringLiteral("ids_info"), QStringLiteral("66106")},
        {QStringLiteral("NavMapScale"), QStringLiteral("1.360000")},
        {QStringLiteral("msg_id_prefix"), QStringLiteral("gcs_refer_system_%1").arg(info.nickname)}
    };
    info.sectorPositions.insert(QStringLiteral("universe"), position);

    updatedUniverse.addSystem(info);

    if (!UniverseSerializer::save(updatedUniverse, universeFilePath)) {
        QFile::remove(absoluteSystemFilePath);
        QDir().rmdir(absoluteSystemDir);
        if (errorMessage)
            *errorMessage = QObject::tr("Universe.ini konnte nicht aktualisiert werden:\n%1").arg(universeFilePath);
        return false;
    }

    if (result) {
        result->systemInfo = info;
        result->absoluteSystemFilePath = absoluteSystemFilePath;
        result->ingameName = request.systemName.trimmed();
    }
    return true;
}

} // namespace flatlas::editors
