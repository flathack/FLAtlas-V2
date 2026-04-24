#include "SystemSettingsService.h"

#include "SystemPersistence.h"
#include "core/PathUtils.h"
#include "domain/SystemDocument.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

using namespace flatlas::infrastructure;

namespace flatlas::editors {

namespace {

QString normalizedSectionName(const QString &name)
{
    return name.trimmed().toLower();
}

int findSectionIndex(const IniDocument &sections, const QString &sectionName)
{
    const QString target = normalizedSectionName(sectionName);
    for (int index = 0; index < sections.size(); ++index) {
        if (normalizedSectionName(sections[index].name) == target)
            return index;
    }
    return -1;
}

QString sectionValue(const IniDocument &sections, const QString &sectionName, const QString &key)
{
    const int index = findSectionIndex(sections, sectionName);
    return index >= 0 ? sections[index].value(key).trimmed() : QString();
}

void setOrClearEntry(QVector<IniEntry> &entries, const QString &key, const QString &value)
{
    for (int index = 0; index < entries.size(); ++index) {
        if (entries[index].first.compare(key, Qt::CaseInsensitive) != 0)
            continue;
        if (value.trimmed().isEmpty())
            entries.removeAt(index);
        else
            entries[index].second = value.trimmed();
        return;
    }

    if (!value.trimmed().isEmpty())
        entries.append({key, value.trimmed()});
}

bool sectionHasEntries(const IniSection &section)
{
    return !section.entries.isEmpty();
}

QStringList scanSystemIniFiles(const QString &gameRoot)
{
    QStringList files;
    const QString systemsDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA/UNIVERSE/SYSTEMS"));
    if (systemsDir.isEmpty())
        return files;

    QDirIterator it(systemsDir, QStringList() << QStringLiteral("*.ini"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        files.append(it.next());
    return files;
}

void appendUniqueSorted(QStringList &values, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return;

    for (const QString &existing : values) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0)
            return;
    }
    values.append(trimmed);
}

QStringList collectMusicValues(const QString &gameRoot, const QString &key)
{
    QStringList values;
    for (const QString &systemFile : scanSystemIniFiles(gameRoot)) {
        const IniDocument doc = IniParser::parseFile(systemFile);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Music"), Qt::CaseInsensitive) != 0)
                continue;
            appendUniqueSorted(values, section.value(key).trimmed());
        }
    }
    std::sort(values.begin(), values.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList collectStarsphereOptions(const QString &gameRoot, const QString &kind)
{
    QStringList values;
    for (const QString &systemFile : scanSystemIniFiles(gameRoot)) {
        const IniDocument doc = IniParser::parseFile(systemFile);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Background"), Qt::CaseInsensitive) != 0)
                continue;
            appendUniqueSorted(values, section.value(kind).trimmed());
        }
    }

    const QString starsphereDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA/SOLAR/STARSPHERE"));
    if (!starsphereDir.isEmpty()) {
        QDirIterator it(starsphereDir,
                        QStringList() << QStringLiteral("*.cmp") << QStringLiteral("*.3db"),
                        QDir::Files);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            const QString relPath = QStringLiteral("solar\\starsphere\\%1").arg(info.fileName());
            const QString stem = info.completeBaseName().toLower();
            if (kind.compare(QStringLiteral("basic_stars"), Qt::CaseInsensitive) == 0) {
                if (stem.contains(QStringLiteral("basic")))
                    appendUniqueSorted(values, relPath);
            } else if (kind.compare(QStringLiteral("complex_stars"), Qt::CaseInsensitive) == 0) {
                if (stem.contains(QStringLiteral("stars")))
                    appendUniqueSorted(values, relPath);
            } else if (kind.compare(QStringLiteral("nebulae"), Qt::CaseInsensitive) == 0) {
                appendUniqueSorted(values, relPath);
            }
        }
    }

    std::sort(values.begin(), values.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList collectDustOptions(const QString &gameRoot)
{
    QStringList values = {
        QStringLiteral("icedust"),
        QStringLiteral("leedsdust"),
        QStringLiteral("asteroiddust"),
        QStringLiteral("debrisdust"),
        QStringLiteral("atmosphere_gray"),
        QStringLiteral("radioactivedust"),
        QStringLiteral("radioactivedust_red"),
        QStringLiteral("lavaashdust"),
        QStringLiteral("snowdust"),
        QStringLiteral("waterdropdust"),
        QStringLiteral("touristdust"),
        QStringLiteral("touristdust_pink"),
        QStringLiteral("touristdust_sienna"),
        QStringLiteral("organismdust"),
        QStringLiteral("attractdust"),
        QStringLiteral("attractdust_orange"),
        QStringLiteral("attractdust_green"),
        QStringLiteral("attractdust_purple"),
        QStringLiteral("oxygendust"),
        QStringLiteral("radioactivedust_blue"),
        QStringLiteral("golddust"),
        QStringLiteral("special_attractdust"),
    };

    for (const QString &systemFile : scanSystemIniFiles(gameRoot)) {
        const IniDocument doc = IniParser::parseFile(systemFile);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Dust"), Qt::CaseInsensitive) == 0)
                appendUniqueSorted(values, section.value(QStringLiteral("spacedust")).trimmed());
        }
    }

    const QString effectsPath = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA/FX/effects.ini"));
    if (!effectsPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(effectsPath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Effect"), Qt::CaseInsensitive) != 0)
                continue;
            if (section.value(QStringLiteral("effect_type")).trimmed().compare(QStringLiteral("EFT_MISC_DUST"), Qt::CaseInsensitive) != 0)
                continue;
            appendUniqueSorted(values, section.value(QStringLiteral("nickname")).trimmed());
        }
    }

    std::sort(values.begin(), values.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return values;
}

QStringList collectFactionDisplayOptions(const QString &gameRoot)
{
    QStringList values;
    const QString initialWorldPath = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA/initialworld.ini"));
    if (initialWorldPath.isEmpty())
        return values;

    flatlas::infrastructure::IdsStringTable ids;
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    const QString lookupDir = exeDir.isEmpty() ? gameRoot : exeDir;
    ids.loadFromFreelancerDir(lookupDir);

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
        const QString label = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
        appendUniqueSorted(values,
                           label.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, label));
    }

    std::sort(values.begin(), values.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return values;
}

} // namespace

SystemSettingsState SystemSettingsService::load(const flatlas::domain::SystemDocument *document)
{
    SystemSettingsState state;
    if (!document)
        return state;

    const IniSection systemInfo = SystemPersistence::systemInfoSection(document);
    const IniDocument extras = SystemPersistence::extraSections(document);

    state.systemNickname = document->name().trimmed();
    state.musicSpace = sectionValue(extras, QStringLiteral("Music"), QStringLiteral("space"));
    state.musicDanger = sectionValue(extras, QStringLiteral("Music"), QStringLiteral("danger"));
    state.musicBattle = sectionValue(extras, QStringLiteral("Music"), QStringLiteral("battle"));
    state.spaceColor = systemInfo.value(QStringLiteral("space_color")).trimmed();
    state.localFaction = systemInfo.value(QStringLiteral("local_faction")).trimmed();
    state.ambientColor = sectionValue(extras, QStringLiteral("Ambient"), QStringLiteral("color"));
    state.dust = sectionValue(extras, QStringLiteral("Dust"), QStringLiteral("spacedust"));
    state.backgroundBasicStars = sectionValue(extras, QStringLiteral("Background"), QStringLiteral("basic_stars"));
    state.backgroundComplexStars = sectionValue(extras, QStringLiteral("Background"), QStringLiteral("complex_stars"));
    state.backgroundNebulae = sectionValue(extras, QStringLiteral("Background"), QStringLiteral("nebulae"));
    return state;
}

SystemSettingsOptions SystemSettingsService::loadOptions(const QString &gameRoot)
{
    SystemSettingsOptions options;
    options.musicSpaceOptions = collectMusicValues(gameRoot, QStringLiteral("space"));
    options.musicDangerOptions = collectMusicValues(gameRoot, QStringLiteral("danger"));
    options.musicBattleOptions = collectMusicValues(gameRoot, QStringLiteral("battle"));
    options.backgroundBasicStarsOptions = collectStarsphereOptions(gameRoot, QStringLiteral("basic_stars"));
    options.backgroundComplexStarsOptions = collectStarsphereOptions(gameRoot, QStringLiteral("complex_stars"));
    options.backgroundNebulaeOptions = collectStarsphereOptions(gameRoot, QStringLiteral("nebulae"));
    options.factionDisplayOptions = collectFactionDisplayOptions(gameRoot);
    options.dustOptions = collectDustOptions(gameRoot);
    return options;
}

bool SystemSettingsService::apply(flatlas::domain::SystemDocument *document,
                                  const SystemSettingsState &state,
                                  QString *errorMessage)
{
    if (!document) {
        if (errorMessage)
            *errorMessage = QObject::tr("Es ist kein System geladen.");
        return false;
    }

    QString normalizedSpaceColor;
    if (!normalizeRgbText(state.spaceColor, &normalizedSpaceColor, errorMessage))
        return false;

    QString normalizedAmbientColor;
    if (!normalizeRgbText(state.ambientColor, &normalizedAmbientColor, errorMessage))
        return false;

    IniSection systemInfo = SystemPersistence::systemInfoSection(document);
    if (systemInfo.name.trimmed().isEmpty())
        systemInfo.name = QStringLiteral("SystemInfo");
    setOrClearEntry(systemInfo.entries, QStringLiteral("space_color"), normalizedSpaceColor);
    setOrClearEntry(systemInfo.entries,
                    QStringLiteral("local_faction"),
                    factionNicknameFromDisplay(state.localFaction));
    SystemPersistence::setSystemInfoSection(document, systemInfo);

    IniDocument extras = SystemPersistence::extraSections(document);

    auto applySection = [&extras](const QString &sectionName,
                                  const QVector<QPair<QString, QString>> &entries) {
        IniSection section;
        const int existingIndex = findSectionIndex(extras, sectionName);
        if (existingIndex >= 0)
            section = extras[existingIndex];
        section.name = sectionName;
        for (const auto &entry : entries)
            setOrClearEntry(section.entries, entry.first, entry.second);

        if (existingIndex >= 0) {
            if (sectionHasEntries(section))
                extras[existingIndex] = section;
            else
                extras.removeAt(existingIndex);
        } else if (sectionHasEntries(section)) {
            extras.append(section);
        }
    };

    applySection(QStringLiteral("Music"), {
        {QStringLiteral("space"), state.musicSpace.trimmed()},
        {QStringLiteral("danger"), state.musicDanger.trimmed()},
        {QStringLiteral("battle"), state.musicBattle.trimmed()},
    });
    applySection(QStringLiteral("Dust"), {
        {QStringLiteral("spacedust"), state.dust.trimmed()},
    });
    applySection(QStringLiteral("Ambient"), {
        {QStringLiteral("color"), normalizedAmbientColor},
    });
    applySection(QStringLiteral("Background"), {
        {QStringLiteral("basic_stars"), state.backgroundBasicStars.trimmed()},
        {QStringLiteral("complex_stars"), state.backgroundComplexStars.trimmed()},
        {QStringLiteral("nebulae"), state.backgroundNebulae.trimmed()},
    });

    SystemPersistence::setExtraSections(document, extras);
    document->setDirty(true);
    return true;
}

bool SystemSettingsService::normalizeRgbText(const QString &rawValue,
                                             QString *normalizedValue,
                                             QString *errorMessage)
{
    const QString trimmed = rawValue.trimmed();
    if (trimmed.isEmpty()) {
        if (normalizedValue)
            *normalizedValue = QString();
        return true;
    }

    const QStringList parts = trimmed.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() != 3) {
        if (errorMessage)
            *errorMessage = QObject::tr("RGB-Farben muessen im Format 'R, G, B' eingegeben werden.");
        return false;
    }

    QStringList normalizedParts;
    normalizedParts.reserve(3);
    for (const QString &part : parts) {
        bool ok = false;
        const int component = part.trimmed().toInt(&ok);
        if (!ok || component < 0 || component > 255) {
            if (errorMessage)
                *errorMessage = QObject::tr("RGB-Farbwerte muessen Ganzzahlen zwischen 0 und 255 sein.");
            return false;
        }
        normalizedParts.append(QString::number(component));
    }

    if (normalizedValue)
        *normalizedValue = normalizedParts.join(QStringLiteral(", "));
    return true;
}

QString SystemSettingsService::factionDisplayForNickname(const QString &nickname,
                                                         const QStringList &displayOptions)
{
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty())
        return QString();

    for (const QString &option : displayOptions) {
        if (factionNicknameFromDisplay(option).compare(trimmed, Qt::CaseInsensitive) == 0)
            return option;
    }
    return trimmed;
}

QString SystemSettingsService::factionNicknameFromDisplay(const QString &rawDisplay)
{
    const QString trimmed = rawDisplay.trimmed();
    const int separator = trimmed.indexOf(QStringLiteral(" - "));
    return separator > 0 ? trimmed.left(separator).trimmed() : trimmed;
}

} // namespace flatlas::editors
