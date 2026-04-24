#include "BaseEditDialog.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"
#include "rendering/view3d/ModelViewport3D.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStackedLayout>
#include <QTabWidget>
#include <QTimer>
#include <QStringListModel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

using flatlas::infrastructure::IdsStringTable;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

QString normalizedKey(const QString &value)
{
    return value.trimmed().toLower();
}

QString primaryGamePath()
{
    return flatlas::core::EditingContext::instance().primaryGamePath();
}

void configureContainsCompleter(QComboBox *combo)
{
    if (!combo)
        return;

    QStringList values;
    for (int index = 0; index < combo->count(); ++index) {
        const QString text = combo->itemText(index).trimmed();
        if (!text.isEmpty())
            values.append(text);
    }
    values.removeDuplicates();

    auto *model = new QStringListModel(values, combo);
    auto *completer = new QCompleter(model, combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    combo->setCompleter(completer);
}

QComboBox *createEditableCombo(const QStringList &values, const QString &current, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItem(QString());
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            combo->addItem(value.trimmed());
    }
    configureContainsCompleter(combo);
    if (!current.trimmed().isEmpty())
        combo->setCurrentText(current.trimmed());
    return combo;
}

QComboBox *createMappedEditableCombo(const QVector<QPair<QString, QString>> &items,
                                     const QString &currentValue,
                                     QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItem(QString(), QString());
    for (const auto &item : items) {
        const QString label = item.first.trimmed();
        const QString value = item.second.trimmed();
        if (!label.isEmpty())
            combo->addItem(label, value);
    }
    configureContainsCompleter(combo);
    if (!currentValue.trimmed().isEmpty()) {
        for (int index = 0; index < combo->count(); ++index) {
            if (combo->itemData(index).toString().compare(currentValue.trimmed(), Qt::CaseInsensitive) == 0) {
                combo->setCurrentIndex(index);
                return combo;
            }
        }
        combo->setCurrentText(currentValue.trimmed());
    }
    return combo;
}

QString comboStoredValue(const QComboBox *combo)
{
    if (!combo)
        return {};
    const QString dataValue = combo->currentData().toString().trimmed();
    if (!dataValue.isEmpty())
        return dataValue;
    const QString text = combo->currentText().trimmed();
    const int separator = text.indexOf(QStringLiteral(" - "));
    return separator >= 0 ? text.left(separator).trimmed() : text;
}

QString comboDataOrText(const QComboBox *combo)
{
    if (!combo)
        return {};
    const QString dataValue = combo->currentData().toString().trimmed();
    if (!dataValue.isEmpty())
        return dataValue;
    return combo->currentText().trimmed();
}

QString normalizedPathKey(const QString &value)
{
    return QDir::fromNativeSeparators(value.trimmed()).toLower();
}

QString roomPreviewKind(const QString &roomName)
{
    const QString room = normalizedKey(roomName);
    if (room == QStringLiteral("equip"))
        return QStringLiteral("equipment");
    if (room == QStringLiteral("ship_dealer") || room == QStringLiteral("ship-dealer"))
        return QStringLiteral("shipdealer");
    return room;
}

QStringList roomPreviewFamiliesForPrefix(const QString &scenePrefix)
{
    const QString prefix = normalizedKey(scenePrefix);
    if (prefix == QStringLiteral("li"))
        return {QStringLiteral("li")};
    if (prefix == QStringLiteral("rh"))
        return {QStringLiteral("rh")};
    if (prefix == QStringLiteral("ku"))
        return {QStringLiteral("ku")};
    if (prefix == QStringLiteral("br"))
        return {QStringLiteral("br")};
    if (prefix == QStringLiteral("bw"))
        return {QStringLiteral("bw"), QStringLiteral("cv"), QStringLiteral("li"), QStringLiteral("br"), QStringLiteral("ku"), QStringLiteral("rh")};
    if (prefix == QStringLiteral("cv"))
        return {QStringLiteral("cv"), QStringLiteral("bw"), QStringLiteral("li"), QStringLiteral("br"), QStringLiteral("ku"), QStringLiteral("rh")};
    if (prefix == QStringLiteral("hi"))
        return {QStringLiteral("hi"), QStringLiteral("co"), QStringLiteral("bw"), QStringLiteral("cv"), QStringLiteral("li"), QStringLiteral("br"), QStringLiteral("ku"), QStringLiteral("rh")};
    if (prefix == QStringLiteral("co"))
        return {QStringLiteral("co"), QStringLiteral("hi"), QStringLiteral("bw"), QStringLiteral("cv"), QStringLiteral("li"), QStringLiteral("br"), QStringLiteral("ku"), QStringLiteral("rh")};
    if (prefix == QStringLiteral("pl"))
        return {QStringLiteral("bw"), QStringLiteral("br"), QStringLiteral("li"), QStringLiteral("ku"), QStringLiteral("rh")};
    if (prefix == QStringLiteral("st"))
        return {QStringLiteral("li"), QStringLiteral("rh"), QStringLiteral("br"), QStringLiteral("ku"), QStringLiteral("bw")};
    return {prefix, QStringLiteral("li"), QStringLiteral("rh"), QStringLiteral("br"), QStringLiteral("ku"), QStringLiteral("bw"), QStringLiteral("cv")};
}

QString resolveBaseRoomPreviewModelPath(const QString &roomName, const QString &scenePath, const QString &gamePath)
{
    static QHash<QString, QString> cache;

    const QString roomKind = roomPreviewKind(roomName);
    const QString sceneKey = normalizedPathKey(scenePath);
    if (roomKind.isEmpty() || sceneKey.isEmpty() || gamePath.trimmed().isEmpty())
        return {};

    const QString cacheKey = normalizedKey(gamePath) + QLatin1Char('|') + roomKind + QLatin1Char('|') + sceneKey;
    if (cache.contains(cacheKey))
        return cache.value(cacheKey);

    const QHash<QString, QHash<QString, QString>> previewCandidates = {
        {QStringLiteral("li"), {{QStringLiteral("deck"), QStringLiteral("DATA/BASES/LIBERTY/li_rockefeller_station_deck.cmp")},
                                 {QStringLiteral("bar"), QStringLiteral("DATA/BASES/LIBERTY/li_manhattan_bar.cmp")},
                                 {QStringLiteral("trader"), QStringLiteral("DATA/BASES/LIBERTY/li_manhattan_trader.cmp")},
                                 {QStringLiteral("equipment"), QStringLiteral("DATA/BASES/LIBERTY/li_manhattan_equip.cmp")},
                                 {QStringLiteral("shipdealer"), QStringLiteral("DATA/BASES/LIBERTY/li_manhattan_shipdealer.cmp")}}},
        {QStringLiteral("rh"), {{QStringLiteral("deck"), QStringLiteral("DATA/BASES/RHEINLAND/rh_starke_station_deck.cmp")},
                                 {QStringLiteral("bar"), QStringLiteral("DATA/BASES/RHEINLAND/rh_berlin_bar.cmp")},
                                 {QStringLiteral("trader"), QStringLiteral("DATA/BASES/RHEINLAND/rh_berlin_trader.cmp")},
                                 {QStringLiteral("equipment"), QStringLiteral("DATA/BASES/RHEINLAND/rh_berlin_equip.cmp")},
                                 {QStringLiteral("shipdealer"), QStringLiteral("DATA/BASES/RHEINLAND/rh_bizmark_shipdealer.cmp")}}},
        {QStringLiteral("ku"), {{QStringLiteral("deck"), QStringLiteral("DATA/BASES/KUSARI/ku_harajuku_station_deck.cmp")},
                                 {QStringLiteral("bar"), QStringLiteral("DATA/BASES/KUSARI/ku_hokkaido_bar.cmp")},
                                 {QStringLiteral("trader"), QStringLiteral("DATA/BASES/KUSARI/ku_hokkaido_trader.cmp")},
                                 {QStringLiteral("equipment"), QStringLiteral("DATA/BASES/KUSARI/ku_hokkaido_equip.cmp")},
                                 {QStringLiteral("shipdealer"), QStringLiteral("DATA/BASES/KUSARI/ku_hokkaido_shipdealer.cmp")}}},
        {QStringLiteral("br"), {{QStringLiteral("deck"), QStringLiteral("DATA/BASES/BRETONIA/br_barbican_station_deck.cmp")},
                                 {QStringLiteral("bar"), QStringLiteral("DATA/BASES/BRETONIA/br_avalon_bar.cmp")},
                                 {QStringLiteral("trader"), QStringLiteral("DATA/BASES/BRETONIA/br_avalon_trader.cmp")},
                                 {QStringLiteral("equipment"), QStringLiteral("DATA/BASES/BRETONIA/br_avalon_equip.cmp")},
                                 {QStringLiteral("shipdealer"), QStringLiteral("DATA/BASES/BRETONIA/br_avalon_shipdealer.cmp")}}},
        {QStringLiteral("bw"), {{QStringLiteral("deck"), QStringLiteral("DATA/BASES/GENERIC/bw_spruage_deck.cmp")},
                                 {QStringLiteral("bar"), QStringLiteral("DATA/INTERFACE/BASESIDE/bar.3db")},
                                 {QStringLiteral("trader"), QStringLiteral("DATA/BASES/LIBERTY/li_manhattan_trader.cmp")},
                                 {QStringLiteral("equipment"), QStringLiteral("DATA/BASES/LIBERTY/li_manhattan_equip.cmp")},
                                 {QStringLiteral("shipdealer"), QStringLiteral("DATA/BASES/BRETONIA/br_avalon_shipdealer.cmp")}}},
        {QStringLiteral("cv"), {{QStringLiteral("bar"), QStringLiteral("DATA/BASES/GENERIC/cv_space_base_bar.cmp")}}},
        {QStringLiteral("hi"), {{QStringLiteral("bar"), QStringLiteral("DATA/BASES/PIRATE/hi_havana_bar.cmp")},
                                 {QStringLiteral("equipment"), QStringLiteral("DATA/BASES/PIRATE/hi_havana_equip.cmp")}}},
        {QStringLiteral("co"), {{QStringLiteral("bar"), QStringLiteral("DATA/BASES/PIRATE/co_curacao_bar.cmp")},
                                 {QStringLiteral("equipment"), QStringLiteral("DATA/BASES/PIRATE/co_curacao_equip.cmp")}}},
    };

    const QString sceneStem = QFileInfo(scenePath).completeBaseName().toLower();
    const QString scenePrefix = sceneStem.contains(QLatin1Char('_')) ? sceneStem.section(QLatin1Char('_'), 0, 0) : QString();
    for (const QString &family : roomPreviewFamiliesForPrefix(scenePrefix)) {
        const QString relativeModelPath = previewCandidates.value(family).value(roomKind);
        if (relativeModelPath.isEmpty())
            continue;
        const QString resolved = flatlas::core::PathUtils::ciResolvePath(gamePath, relativeModelPath);
        if (!resolved.isEmpty() && QFileInfo::exists(resolved)) {
            cache.insert(cacheKey, resolved);
            return resolved;
        }
    }

    const QHash<QString, QString> fallbackPatterns = {
        {QStringLiteral("deck"), QStringLiteral("*_deck.cmp")},
        {QStringLiteral("bar"), QStringLiteral("*_bar.cmp")},
        {QStringLiteral("trader"), QStringLiteral("*_trader.cmp")},
        {QStringLiteral("equipment"), QStringLiteral("*_equip.cmp")},
        {QStringLiteral("shipdealer"), QStringLiteral("*_shipdealer.cmp")},
    };
    const QString basesDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA/BASES"));
    const QString pattern = fallbackPatterns.value(roomKind);
    if (!basesDir.isEmpty() && !pattern.isEmpty()) {
        QDirIterator it(basesDir, {pattern}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            const QString candidate = it.next();
            cache.insert(cacheKey, candidate);
            return candidate;
        }
    }

    cache.insert(cacheKey, QString());
    return {};
}

QString roomKeyFromRoomTemplateFile(const QString &fileName)
{
    const QString stem = QFileInfo(fileName).completeBaseName().trimmed().toLower();
    for (const QString &roomKey : {QStringLiteral("shipdealer"),
                                   QStringLiteral("cityscape"),
                                   QStringLiteral("equipment"),
                                   QStringLiteral("trader"),
                                   QStringLiteral("bar"),
                                   QStringLiteral("deck")}) {
        if (stem.endsWith(QStringLiteral("_") + roomKey))
            return roomKey;
    }
    return {};
}

QString extractRoomScenePath(const QString &content)
{
    bool inRoomInfo = false;
    for (const QString &rawLine : content.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inRoomInfo = line.mid(1, line.size() - 2).trimmed().compare(QStringLiteral("Room_Info"), Qt::CaseInsensitive) == 0;
            continue;
        }
        if (!inRoomInfo)
            continue;
        const int separator = line.indexOf(QLatin1Char('='));
        if (separator < 0)
            continue;
        if (line.left(separator).trimmed().compare(QStringLiteral("scene"), Qt::CaseInsensitive) != 0)
            continue;
        const QString value = line.mid(separator + 1).trimmed();
        const QStringList parts = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        return parts.isEmpty() ? value : parts.constLast().trimmed();
    }
    return {};
}

QString roomSceneDisplayLabel(const QString &templateNickname, const QString &scenePath)
{
    const QString nickname = templateNickname.trimmed();
    const QString scene = scenePath.trimmed();
    if (nickname.isEmpty())
        return scene;
    return scene.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, scene);
}

int previewSidebarPreferredWidth(QWidget *dialog)
{
    const int dialogWidth = dialog ? dialog->width() : 1240;
    return std::clamp(dialogWidth / 3, 320, 420);
}

struct BaseDialogCatalog {
    QStringList archetypes;
    QStringList loadouts;
    QVector<QPair<QString, QString>> factions;
    QStringList pilots;
    QStringList voices;
    QStringList heads;
    QStringList bodies;
    QVector<QPair<QString, QString>> templateBases;
    QHash<QString, QString> archetypeModelPaths;
    QHash<QString, QVector<QPair<QString, QString>>> roomSceneOptionsByRoom;
};

const BaseDialogCatalog &sharedBaseDialogCatalog()
{
    static QString cachedGameKey;
    static BaseDialogCatalog catalog;

    const QString gamePath = primaryGamePath();
    const QString gameKey = normalizedKey(gamePath);
    if (cachedGameKey == gameKey)
        return catalog;

    cachedGameKey = gameKey;
    catalog = {};
    catalog.voices = {
        QStringLiteral("atc_leg_m01"),
        QStringLiteral("atc_leg_f01"),
        QStringLiteral("atc_leg_f01a"),
        QStringLiteral("mc_leg_m01"),
    };

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        return catalog;

    const QString bodypartsPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("CHARACTERS/bodyparts.ini"));
    if (!bodypartsPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(bodypartsPath);
        QSet<QString> seenHeads;
        QSet<QString> seenBodies;
        for (const IniSection &section : doc) {
            const QString sectionName = normalizedKey(section.name);
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty())
                continue;
            if (sectionName == QStringLiteral("head") && !seenHeads.contains(key)) {
                seenHeads.insert(key);
                catalog.heads.append(nickname);
            } else if (sectionName == QStringLiteral("body") && !seenBodies.contains(key)) {
                seenBodies.insert(key);
                catalog.bodies.append(nickname);
            }
        }
        std::sort(catalog.heads.begin(), catalog.heads.end(), [](const QString &left, const QString &right) {
            return left.compare(right, Qt::CaseInsensitive) < 0;
        });
        std::sort(catalog.bodies.begin(), catalog.bodies.end(), [](const QString &left, const QString &right) {
            return left.compare(right, Qt::CaseInsensitive) < 0;
        });
    }

    const QString solarArchPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
    if (!solarArchPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(solarArchPath);
        QSet<QString> seen;
        for (const IniSection &section : doc) {
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;

            const QString key = normalizedKey(nickname);
            if (!seen.contains(key)) {
                seen.insert(key);
                catalog.archetypes.append(nickname);
            }

            const QString relativeModelPath = section.value(QStringLiteral("DA_archetype")).trimmed();
            if (!relativeModelPath.isEmpty()) {
                const QString modelPath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativeModelPath);
                if (!modelPath.isEmpty())
                    catalog.archetypeModelPaths.insert(key, modelPath);
            }
        }
        std::sort(catalog.archetypes.begin(), catalog.archetypes.end(), [](const QString &left, const QString &right) {
            return left.compare(right, Qt::CaseInsensitive) < 0;
        });
    }

    const QStringList loadoutSources = {
        QStringLiteral("SHIPS/loadouts.ini"),
        QStringLiteral("SHIPS/loadouts_special.ini"),
        QStringLiteral("SOLAR/loadouts.ini"),
    };
    QSet<QString> seenLoadouts;
    for (const QString &relativePath : loadoutSources) {
        const QString absolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
        if (absolutePath.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(absolutePath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Loadout"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty() || seenLoadouts.contains(key))
                continue;
            seenLoadouts.insert(key);
            catalog.loadouts.append(nickname);
        }
    }
    std::sort(catalog.loadouts.begin(), catalog.loadouts.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("EXE"));
    IdsStringTable ids;
    if (!exeDir.isEmpty())
        ids.loadFromFreelancerDir(exeDir);

    const QString initialWorldPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("initialworld.ini"));
    if (!initialWorldPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(initialWorldPath);
        QSet<QString> seenFactions;
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Group"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;
            const QString key = normalizedKey(nickname);
            if (seenFactions.contains(key))
                continue;
            seenFactions.insert(key);

            bool ok = false;
            const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt(&ok);
            const QString displayName = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
            const QString label = displayName.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, displayName);
            catalog.factions.append({label, nickname});
        }
        std::sort(catalog.factions.begin(), catalog.factions.end(), [](const auto &left, const auto &right) {
            return left.first.compare(right.first, Qt::CaseInsensitive) < 0;
        });
    }

    const QString missionsDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS"));
    const QStringList pilotSources = {
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_population.ini")),
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_story.ini")),
    };
    QSet<QString> seenPilots;
    for (const QString &pilotSource : pilotSources) {
        if (pilotSource.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(pilotSource);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Pilot"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty() || seenPilots.contains(key) || !key.startsWith(QStringLiteral("pilot_solar")))
                continue;
            seenPilots.insert(key);
            catalog.pilots.append(nickname);
        }
    }
    std::sort(catalog.pilots.begin(), catalog.pilots.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    const QString universeDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE"));
    if (!universeDir.isEmpty()) {
        QHash<QString, QHash<QString, QString>> sceneLabelsByRoom;
        QDirIterator it(universeDir, {QStringLiteral("*.ini")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString absolutePath = it.next();
            const QFileInfo fileInfo(absolutePath);
            if (fileInfo.dir().dirName().compare(QStringLiteral("ROOMS"), Qt::CaseInsensitive) != 0)
                continue;

            const QString roomKey = roomKeyFromRoomTemplateFile(fileInfo.fileName());
            if (roomKey.isEmpty())
                continue;

            QFile file(absolutePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString scenePath = extractRoomScenePath(QString::fromUtf8(file.readAll()));
            if (scenePath.trimmed().isEmpty())
                continue;

            const QString label = roomSceneDisplayLabel(fileInfo.completeBaseName(), scenePath);
            sceneLabelsByRoom[roomKey].insert(label, scenePath.trimmed());
        }

        for (auto it = sceneLabelsByRoom.cbegin(); it != sceneLabelsByRoom.cend(); ++it) {
            QVector<QPair<QString, QString>> options;
            options.reserve(it.value().size());
            for (auto labelIt = it.value().cbegin(); labelIt != it.value().cend(); ++labelIt)
                options.append({labelIt.key(), labelIt.value()});
            std::sort(options.begin(), options.end(), [](const auto &left, const auto &right) {
                return left.first.compare(right.first, Qt::CaseInsensitive) < 0;
            });
            catalog.roomSceneOptionsByRoom.insert(it.key(), options);
        }
    }

    const QString universePath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    if (!universePath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(universePath);
        QSet<QString> seenTemplates;
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty() || seenTemplates.contains(key))
                continue;
            seenTemplates.insert(key);
            bool ok = false;
            const int idsName = section.value(QStringLiteral("strid_name")).trimmed().toInt(&ok);
            const QString ingameName = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
            const QString label = ingameName.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, ingameName);
            catalog.templateBases.append({label, nickname});
        }
        std::sort(catalog.templateBases.begin(), catalog.templateBases.end(), [](const auto &left, const auto &right) {
            return left.first.compare(right.first, Qt::CaseInsensitive) < 0;
        });
    }

    return catalog;
}

QWidget *createPreviewFrame(flatlas::rendering::ModelViewport3D **outPreview,
                            QLabel **outFallback,
                            QStackedLayout **outStack,
                            QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *host = new QWidget(frame);
    auto *stack = new QStackedLayout(host);
    stack->setContentsMargins(0, 0, 0, 0);

    auto *preview = new flatlas::rendering::ModelViewport3D(host);
    stack->addWidget(preview);

    auto *fallback = new QLabel(QObject::tr("Waehle einen Archetype um die Vorschau zu laden."), host);
    fallback->setAlignment(Qt::AlignCenter);
    fallback->setWordWrap(true);
    fallback->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    stack->addWidget(fallback);
    stack->setCurrentWidget(fallback);

    layout->addWidget(host, 1);

    if (outPreview)
        *outPreview = preview;
    if (outFallback)
        *outFallback = fallback;
    if (outStack)
        *outStack = stack;
    return frame;
}

} // namespace

namespace flatlas::editors {

BaseEditDialog::BaseEditDialog(const BaseEditState &state,
                               const QHash<QString, QString> &textOverrides,
                               QWidget *parent)
    : QDialog(parent)
    , m_initialState(state)
    , m_textOverrides(textOverrides)
    , m_roomStates(state.rooms)
{
    setWindowTitle(state.editMode ? tr("Base bearbeiten") : tr("Base erstellen"));
    resize(1240, 760);

    const BaseDialogCatalog &catalog = sharedBaseDialogCatalog();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs, 1);

    auto *generalTab = new QWidget(m_tabs);
    auto *generalSplitter = new QSplitter(Qt::Horizontal, generalTab);
    generalSplitter->setChildrenCollapsible(false);
    auto *generalTabLayout = new QVBoxLayout(generalTab);
    generalTabLayout->setContentsMargins(0, 0, 0, 0);
    generalTabLayout->addWidget(generalSplitter, 1);

    auto *generalScroll = new QScrollArea(generalSplitter);
    generalScroll->setWidgetResizable(true);
    auto *generalContent = new QWidget(generalScroll);
    auto *contentLayout = new QVBoxLayout(generalContent);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(12);
    generalScroll->setWidget(generalContent);
    generalSplitter->addWidget(generalScroll);

    auto *generalPreviewSidebar = new QWidget(generalSplitter);
    auto *generalPreviewLayout = new QVBoxLayout(generalPreviewSidebar);
    generalPreviewLayout->setContentsMargins(12, 12, 12, 12);
    generalPreviewLayout->setSpacing(8);
    generalSplitter->addWidget(generalPreviewSidebar);
    generalSplitter->setStretchFactor(0, 7);
    generalSplitter->setStretchFactor(1, 5);
    generalPreviewSidebar->setMinimumWidth(320);
    generalPreviewSidebar->setMaximumWidth(420);
    QTimer::singleShot(0, this, [this, generalSplitter, generalPreviewSidebar]() {
        const int previewWidth = previewSidebarPreferredWidth(this);
        generalPreviewSidebar->setMinimumWidth(previewWidth);
        generalPreviewSidebar->setMaximumWidth(previewWidth);
        generalSplitter->setSizes({std::max(720, width() - previewWidth - 48), previewWidth});
    });

    auto *baseGroup = new QGroupBox(tr("Base"), generalContent);
    auto *baseForm = new QFormLayout(baseGroup);
    m_baseNicknameEdit = new QLineEdit(state.baseNickname, baseGroup);
    m_objectNicknameEdit = new QLineEdit(state.objectNickname, baseGroup);
    m_displayNameEdit = new QLineEdit(state.displayName, baseGroup);
    if (state.editMode) {
        m_baseNicknameEdit->setReadOnly(true);
        m_objectNicknameEdit->setReadOnly(true);
    }
    baseForm->addRow(tr("Base Nickname:"), m_baseNicknameEdit);
    baseForm->addRow(tr("Objekt Nickname:"), m_objectNicknameEdit);
    baseForm->addRow(tr("Name:"), m_displayNameEdit);
    contentLayout->addWidget(baseGroup);

    auto *objectGroup = new QGroupBox(tr("Space-Objekt"), generalContent);
    auto *objectForm = new QFormLayout(objectGroup);
    m_archetypeCombo = createEditableCombo(catalog.archetypes, state.archetype, objectGroup);
    m_loadoutCombo = createEditableCombo(catalog.loadouts, state.loadout, objectGroup);
    m_reputationCombo = createMappedEditableCombo(catalog.factions, state.reputation, objectGroup);
    m_pilotCombo = createEditableCombo(catalog.pilots, state.pilot, objectGroup);
    m_voiceCombo = createEditableCombo(catalog.voices, state.voice, objectGroup);
    m_headCombo = createEditableCombo(catalog.heads, state.head.isEmpty() ? QStringLiteral("benchmark_male_head") : state.head, objectGroup);
    m_bodyCombo = createEditableCombo(catalog.bodies, state.body.isEmpty() ? QStringLiteral("benchmark_male_body") : state.body, objectGroup);
    if (state.editMode)
        m_archetypeCombo->setEnabled(false);

    objectForm->addRow(tr("Archetype:"), m_archetypeCombo);
    objectForm->addRow(tr("Loadout:"), m_loadoutCombo);
    objectForm->addRow(tr("Reputation:"), m_reputationCombo);
    objectForm->addRow(tr("Pilot:"), m_pilotCombo);
    objectForm->addRow(tr("Voice:"), m_voiceCombo);
    auto *costumeGroup = new QGroupBox(tr("Space Costume"), objectGroup);
    auto *costumeForm = new QFormLayout(costumeGroup);
    costumeForm->addRow(tr("Head:"), m_headCombo);
    costumeForm->addRow(tr("Body:"), m_bodyCombo);
    objectForm->addRow(costumeGroup);
    contentLayout->addWidget(objectGroup);

    auto *universeGroup = new QGroupBox(tr("Universe"), generalContent);
    auto *universeForm = new QFormLayout(universeGroup);
    m_bgcsEdit = new QLineEdit(state.bgcsBaseRunBy, universeGroup);
    universeForm->addRow(tr("BGCS_base_run_by:"), m_bgcsEdit);
    contentLayout->addWidget(universeGroup);

    auto *infocardGroup = new QGroupBox(tr("ids_info"), generalContent);
    auto *infocardLayout = new QVBoxLayout(infocardGroup);
    m_infocardEdit = new QPlainTextEdit(infocardGroup);
    m_infocardEdit->setPlainText(state.infocardXml);
    m_infocardEdit->setPlaceholderText(tr("ids_info Text"));
    infocardLayout->addWidget(m_infocardEdit);
    contentLayout->addWidget(infocardGroup);
    contentLayout->addStretch(1);

    auto *generalPreviewTitle = new QLabel(tr("3D Preview"), generalPreviewSidebar);
    generalPreviewTitle->setStyleSheet(QStringLiteral("font-weight:600;"));
    auto *generalPreviewHelp = new QLabel(tr("Die Vorschau zeigt den aktuell gewaehlten Base-Archetype."), generalPreviewSidebar);
    generalPreviewHelp->setWordWrap(true);
    generalPreviewLayout->addWidget(generalPreviewTitle);
    generalPreviewLayout->addWidget(generalPreviewHelp);
    generalPreviewLayout->addWidget(createPreviewFrame(&m_preview, &m_previewFallback, &m_previewStack, generalPreviewSidebar), 1);
    m_tabs->addTab(generalTab, tr("General"));

    auto *roomsTab = new QWidget(m_tabs);
    auto *roomsSplitter = new QSplitter(Qt::Horizontal, roomsTab);
    roomsSplitter->setChildrenCollapsible(false);
    auto *roomsTabLayout = new QVBoxLayout(roomsTab);
    roomsTabLayout->setContentsMargins(0, 0, 0, 0);
    roomsTabLayout->addWidget(roomsSplitter, 1);

    auto *roomsContent = new QWidget(roomsSplitter);
    auto *roomsContentLayout = new QVBoxLayout(roomsContent);
    roomsContentLayout->setContentsMargins(12, 12, 12, 12);
    roomsContentLayout->setSpacing(12);
    roomsSplitter->addWidget(roomsContent);

    auto *roomsPreviewSidebar = new QWidget(roomsSplitter);
    auto *roomsPreviewLayout = new QVBoxLayout(roomsPreviewSidebar);
    roomsPreviewLayout->setContentsMargins(12, 12, 12, 12);
    roomsPreviewLayout->setSpacing(8);
    roomsSplitter->addWidget(roomsPreviewSidebar);
    roomsSplitter->setStretchFactor(0, 7);
    roomsSplitter->setStretchFactor(1, 5);
    roomsPreviewSidebar->setMinimumWidth(320);
    roomsPreviewSidebar->setMaximumWidth(420);
    QTimer::singleShot(0, this, [this, roomsSplitter, roomsPreviewSidebar]() {
        const int previewWidth = previewSidebarPreferredWidth(this);
        roomsPreviewSidebar->setMinimumWidth(previewWidth);
        roomsPreviewSidebar->setMaximumWidth(previewWidth);
        roomsSplitter->setSizes({std::max(720, width() - previewWidth - 48), previewWidth});
    });

    auto *roomsGroup = new QGroupBox(tr("Rooms"), roomsContent);
    auto *roomsLayout = new QVBoxLayout(roomsGroup);
    auto *templateForm = new QFormLayout();
    m_templateCombo = createMappedEditableCombo(catalog.templateBases, QString(), roomsGroup);
    m_copyNpcsCheck = new QCheckBox(tr("Copy NPCs"), roomsGroup);
    m_copyNpcsCheck->setChecked(true);
    m_randomNpcAppearanceCheck = new QCheckBox(tr("Random NPC head/body"), roomsGroup);
    m_templateInfoLabel = new QLabel(QString(), roomsGroup);
    m_templateInfoLabel->setWordWrap(true);
    m_templateInfoLabel->setStyleSheet(QStringLiteral("color:#9aa3ad;"));
    templateForm->addRow(tr("Room Template kopieren:"), m_templateCombo);
    templateForm->addRow(QString(), m_copyNpcsCheck);
    templateForm->addRow(QString(), m_randomNpcAppearanceCheck);
    templateForm->addRow(QString(), m_templateInfoLabel);
    roomsLayout->addLayout(templateForm);

    m_roomTable = new QTableWidget(0, 4, roomsGroup);
    m_roomTable->setHorizontalHeaderLabels({tr("Use"), tr("Activate"), tr("Room"), tr("Scene")});
    m_roomTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_roomTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_roomTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_roomTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_roomTable->verticalHeader()->setVisible(false);
    m_roomTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_roomTable->setSelectionMode(QAbstractItemView::SingleSelection);
    roomsLayout->addWidget(m_roomTable, 1);

    auto *roomButtons = new QHBoxLayout();
    m_addRoomButton = new QPushButton(tr("Raum hinzufuegen"), roomsGroup);
    m_removeRoomButton = new QPushButton(tr("Ausgewaehlten Raum entfernen"), roomsGroup);
    roomButtons->addWidget(m_addRoomButton);
    roomButtons->addWidget(m_removeRoomButton);
    roomButtons->addStretch(1);
    roomsLayout->addLayout(roomButtons);

    roomsLayout->addWidget(new QLabel(tr("Fixture-NPCs des aktuell ausgewaehlten Raums."), roomsGroup));

    m_npcTable = new QTableWidget(0, 2, roomsGroup);
    m_npcTable->setHorizontalHeaderLabels({tr("NPC"), tr("Rolle")});
    m_npcTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_npcTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_npcTable->verticalHeader()->setVisible(false);
    m_npcTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_npcTable->setSelectionMode(QAbstractItemView::SingleSelection);
    roomsLayout->addWidget(m_npcTable, 1);

    auto *npcButtons = new QHBoxLayout();
    m_addNpcButton = new QPushButton(tr("NPC hinzufuegen"), roomsGroup);
    m_removeNpcButton = new QPushButton(tr("Ausgewaehlten NPC entfernen"), roomsGroup);
    npcButtons->addWidget(m_addNpcButton);
    npcButtons->addWidget(m_removeNpcButton);
    npcButtons->addStretch(1);
    roomsLayout->addLayout(npcButtons);

    auto *roomsMetaForm = new QFormLayout();
    m_startRoomCombo = new QComboBox(roomsGroup);
    m_priceVarianceSpin = new QDoubleSpinBox(roomsGroup);
    m_priceVarianceSpin->setRange(0.0, 1.0);
    m_priceVarianceSpin->setSingleStep(0.05);
    m_priceVarianceSpin->setDecimals(2);
    m_priceVarianceSpin->setValue(state.priceVariance);
    roomsMetaForm->addRow(tr("Start-Raum"), m_startRoomCombo);
    roomsMetaForm->addRow(tr("Price Variance"), m_priceVarianceSpin);
    roomsLayout->addLayout(roomsMetaForm);
    roomsContentLayout->addWidget(roomsGroup, 1);

    auto *roomPreviewTitle = new QLabel(tr("Room Preview"), roomsPreviewSidebar);
    roomPreviewTitle->setStyleSheet(QStringLiteral("font-weight:600;"));
    auto *roomPreviewHelp = new QLabel(tr("Die Vorschau zeigt den aktuell ausgewaehlten Raum. Falls kein Modell aufloesbar ist, bleibt die Room-Bearbeitung unveraendert moeglich."), roomsPreviewSidebar);
    roomPreviewHelp->setWordWrap(true);
    m_selectedRoomLabel = new QLabel(tr("Kein Raum ausgewaehlt"), roomsPreviewSidebar);
    m_selectedRoomLabel->setWordWrap(true);
    m_activeRoomLabel = new QLabel(tr("Aktiver Raum im Viewer: -"), roomsPreviewSidebar);
    m_activeRoomLabel->setWordWrap(true);
    roomsPreviewLayout->addWidget(roomPreviewTitle);
    roomsPreviewLayout->addWidget(roomPreviewHelp);
    roomsPreviewLayout->addWidget(m_selectedRoomLabel);
    roomsPreviewLayout->addWidget(m_activeRoomLabel);
    roomsPreviewLayout->addWidget(createPreviewFrame(&m_roomPreview, &m_roomPreviewFallback, &m_roomPreviewStack, roomsPreviewSidebar), 1);
    m_tabs->addTab(roomsTab, tr("Rooms"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_addRoomButton, &QPushButton::clicked, this, &BaseEditDialog::addRoom);
    connect(m_removeRoomButton, &QPushButton::clicked, this, &BaseEditDialog::removeSelectedRoom);
    connect(m_roomTable, &QTableWidget::itemChanged, this, &BaseEditDialog::onRoomItemChanged);
    connect(m_roomTable, &QTableWidget::itemSelectionChanged, this, &BaseEditDialog::onRoomSelectionChanged);
    connect(m_npcTable, &QTableWidget::itemChanged, this, &BaseEditDialog::onNpcItemChanged);
    connect(m_addNpcButton, &QPushButton::clicked, this, &BaseEditDialog::addNpc);
    connect(m_removeNpcButton, &QPushButton::clicked, this, &BaseEditDialog::removeSelectedNpc);
    connect(m_archetypeCombo, &QComboBox::currentTextChanged, this, [this]() {
        applyArchetypeDefaults();
        refreshPreview();
    });
    connect(m_templateCombo, &QComboBox::currentTextChanged, this, [this]() { applyTemplateSelection(); });
    connect(m_copyNpcsCheck, &QCheckBox::toggled, this, [this]() { applyTemplateSelection(); });

    m_selectedRoomKey = m_roomStates.value(0).roomName.trimmed();
    m_activeRoomKey = !state.startRoom.trimmed().isEmpty() ? state.startRoom.trimmed() : m_selectedRoomKey;
    populateRooms(m_roomStates);
    refreshStartRooms();
    if (!state.startRoom.trimmed().isEmpty())
        m_startRoomCombo->setCurrentText(state.startRoom.trimmed());
    setSelectedRoom(m_selectedRoomKey);
    setActiveRoom(m_activeRoomKey);
    m_lastSuggestedLoadout = state.loadout.trimmed();
    m_lastSuggestedIdsInfo = state.infocardXml.trimmed();
    if (state.editMode) {
        m_templateCombo->setEnabled(false);
        m_copyNpcsCheck->setEnabled(false);
        m_randomNpcAppearanceCheck->setEnabled(false);
    }
    refreshPreview();
    updateRoomSelectionUi();
    refreshRoomPreview();
}

void BaseEditDialog::populateRooms(const QVector<BaseRoomState> &rooms)
{
    const QString previousSelected = m_selectedRoomKey;
    m_roomTable->blockSignals(true);
    m_roomTable->setRowCount(0);
    for (const BaseRoomState &room : rooms) {
        const int row = m_roomTable->rowCount();
        m_roomTable->insertRow(row);

        auto *enabledItem = new QTableWidgetItem();
        enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        enabledItem->setCheckState(room.enabled ? Qt::Checked : Qt::Unchecked);
        m_roomTable->setItem(row, 0, enabledItem);

        auto *activateButton = new QPushButton(tr("Activate"), m_roomTable);
        activateButton->setProperty("roomRow", row);
        connect(activateButton, &QPushButton::clicked, this, &BaseEditDialog::activateRoomFromTable);
        m_roomTable->setCellWidget(row, 1, activateButton);

        auto *roomItem = new QTableWidgetItem(room.roomName);
        m_roomTable->setItem(row, 2, roomItem);
        populateSceneCombo(row, room.roomName, room.scenePath);
    }
    m_roomTable->blockSignals(false);

    const int selectedRow = findRoomRowByName(previousSelected);
    if (selectedRow >= 0) {
        QSignalBlocker selectionBlocker(m_roomTable->selectionModel());
        m_roomTable->selectRow(selectedRow);
    }
    updateRoomActivationUi();
}

BaseRoomState BaseEditDialog::roomFromRow(int row) const
{
    BaseRoomState room;
    if (const auto *enabledItem = m_roomTable->item(row, 0))
        room.enabled = enabledItem->checkState() == Qt::Checked;
    if (const auto *roomItem = m_roomTable->item(row, 2))
        room.roomName = roomItem->text().trimmed();
    if (auto *sceneCombo = qobject_cast<QComboBox *>(m_roomTable->cellWidget(row, 3)))
        room.scenePath = comboDataOrText(sceneCombo);
    return room;
}

BaseEditState BaseEditDialog::state() const
{
    BaseEditState result = m_initialState;
    result.objectNickname = m_objectNicknameEdit->text().trimmed();
    result.baseNickname = m_baseNicknameEdit->text().trimmed();
    result.archetype = m_archetypeCombo->currentText().trimmed();
    result.loadout = m_loadoutCombo->currentText().trimmed();
    result.reputation = comboStoredValue(m_reputationCombo);
    result.pilot = m_pilotCombo->currentText().trimmed();
    result.voice = m_voiceCombo->currentText().trimmed();
    result.head = m_headCombo->currentText().trimmed();
    result.body = m_bodyCombo->currentText().trimmed();
    result.bgcsBaseRunBy = m_bgcsEdit->text().trimmed();
    result.displayName = m_displayNameEdit->text().trimmed();
    result.startRoom = m_startRoomCombo->currentText().trimmed();
    result.priceVariance = m_priceVarianceSpin->value();
    result.infocardXml = m_infocardEdit->toPlainText().trimmed();
    result.rooms = m_roomStates;
    return result;
}

void BaseEditDialog::refreshStartRooms()
{
    const QString current = m_startRoomCombo->currentText().trimmed();
    QStringList rooms;
    for (int row = 0; row < m_roomTable->rowCount(); ++row) {
        const BaseRoomState room = roomFromRow(row);
        if (room.enabled && !room.roomName.trimmed().isEmpty())
            rooms.append(room.roomName.trimmed());
    }
    rooms.removeDuplicates();

    m_startRoomCombo->blockSignals(true);
    m_startRoomCombo->clear();
    m_startRoomCombo->addItems(rooms);
    if (!current.isEmpty() && rooms.contains(current, Qt::CaseInsensitive))
        m_startRoomCombo->setCurrentText(current);
    else if (rooms.contains(QStringLiteral("Deck")))
        m_startRoomCombo->setCurrentText(QStringLiteral("Deck"));
    else if (rooms.contains(QStringLiteral("Cityscape")))
        m_startRoomCombo->setCurrentText(QStringLiteral("Cityscape"));
    else if (!rooms.isEmpty())
        m_startRoomCombo->setCurrentIndex(0);
    m_startRoomCombo->blockSignals(false);
}

void BaseEditDialog::addRoom()
{
    bool ok = false;
    const QString roomName = QInputDialog::getText(this,
                                                   tr("Raum hinzufuegen"),
                                                   tr("Raumname"),
                                                   QLineEdit::Normal,
                                                   QStringLiteral("Deck"),
                                                   &ok)
        .trimmed();
    if (!ok || roomName.isEmpty())
        return;

    BaseRoomState room;
    room.enabled = true;
    room.roomName = roomName;
    m_roomStates.append(room);
    m_selectedRoomKey = roomName;
    populateRooms(m_roomStates);
    setSelectedRoom(roomName);
    refreshStartRooms();
}

void BaseEditDialog::removeSelectedRoom()
{
    const int row = selectedRoomRow();
    if (row < 0)
        return;

    const QString removedRoom = m_roomStates.at(row).roomName.trimmed();
    QString replacementSelection;
    if (row + 1 < m_roomStates.size())
        replacementSelection = m_roomStates.at(row + 1).roomName.trimmed();
    else if (row > 0)
        replacementSelection = m_roomStates.at(row - 1).roomName.trimmed();

    m_roomStates.removeAt(row);
    if (normalizedKey(m_selectedRoomKey) == normalizedKey(removedRoom))
        m_selectedRoomKey = replacementSelection;
    if (normalizedKey(m_activeRoomKey) == normalizedKey(removedRoom))
        m_activeRoomKey = replacementSelection;
    populateRooms(m_roomStates);
    setSelectedRoom(m_selectedRoomKey);
    refreshStartRooms();
}

void BaseEditDialog::onRoomSelectionChanged()
{
    const int row = m_roomTable->currentRow();
    if (row >= 0 && row < m_roomStates.size())
        m_selectedRoomKey = m_roomStates.at(row).roomName.trimmed();
    populateNpcTable(row);
    updateRoomSelectionUi();
    refreshRoomPreview();
}

void BaseEditDialog::onRoomItemChanged(QTableWidgetItem *item)
{
    if (!item)
        return;

    const int row = item->row();
    if (row < 0 || row >= m_roomStates.size())
        return;

    BaseRoomState updated = m_roomStates.at(row);
    const QString previousRoomName = updated.roomName.trimmed();
    const BaseRoomState visibleState = roomFromRow(row);
    updated.enabled = visibleState.enabled;
    updated.roomName = visibleState.roomName;
    updated.scenePath = visibleState.scenePath;
    m_roomStates[row] = updated;
    if (normalizedKey(m_selectedRoomKey) == normalizedKey(previousRoomName))
        m_selectedRoomKey = updated.roomName.trimmed();
    if (normalizedKey(m_activeRoomKey) == normalizedKey(previousRoomName))
        m_activeRoomKey = updated.roomName.trimmed();
    if (item->column() == 2)
        populateSceneCombo(row, m_roomStates[row].roomName, m_roomStates[row].scenePath);
    refreshStartRooms();
    if (row == m_roomTable->currentRow())
        populateNpcTable(row);
    updateRoomSelectionUi();
    updateRoomActivationUi();
    refreshRoomPreview();
}
void BaseEditDialog::activateRoomFromTable()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;
    bool ok = false;
    const int row = button->property("roomRow").toInt(&ok);
    if (!ok || row < 0 || row >= m_roomStates.size())
        return;

    const BaseRoomState room = roomFromRow(row);
    const QString modelPath = resolveBaseRoomPreviewModelPath(room.roomName,
                                                              room.scenePath,
                                                              flatlas::core::EditingContext::instance().primaryGamePath());
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        QMessageBox::warning(this,
                             tr("Room aktivieren"),
                             tr("Fuer %1 konnte kein darstellbares Interior-Modell gefunden werden.")
                                 .arg(room.roomName.trimmed().isEmpty() ? tr("den ausgewaehlten Raum") : room.roomName.trimmed()));
        return;
    }

    setActiveRoom(room.roomName);
    refreshRoomPreview();
}


void BaseEditDialog::onNpcItemChanged(QTableWidgetItem *item)
{
    Q_UNUSED(item);
    syncNpcStateFromTable();
}

void BaseEditDialog::addNpc()
{
    const int roomRow = m_roomTable->currentRow();
    if (roomRow < 0 || roomRow >= m_roomStates.size())
        return;

    BaseRoomNpcState npc;
    npc.role = roleChoicesForRoom(m_roomStates.at(roomRow).roomName).value(0);
    m_roomStates[roomRow].npcs.append(npc);
    populateNpcTable(roomRow);
}

void BaseEditDialog::removeSelectedNpc()
{
    const int roomRow = m_roomTable->currentRow();
    const int npcRow = m_npcTable ? m_npcTable->currentRow() : -1;
    if (roomRow < 0 || roomRow >= m_roomStates.size() || npcRow < 0 || npcRow >= m_roomStates.at(roomRow).npcs.size())
        return;

    m_roomStates[roomRow].npcs.removeAt(npcRow);
    populateNpcTable(roomRow);
}

void BaseEditDialog::populateNpcTable(int roomRow)
{
    if (!m_npcTable)
        return;

    m_npcTable->blockSignals(true);
    m_npcTable->setRowCount(0);

    const bool validRoom = roomRow >= 0 && roomRow < m_roomStates.size();
    m_npcTable->setEnabled(validRoom);
    if (m_addNpcButton)
        m_addNpcButton->setEnabled(validRoom);
    if (m_removeNpcButton)
        m_removeNpcButton->setEnabled(validRoom && !m_roomStates.at(roomRow).npcs.isEmpty());
    if (!validRoom) {
        m_npcTable->blockSignals(false);
        return;
    }

    const QStringList roleChoices = roleChoicesForRoom(m_roomStates.at(roomRow).roomName);
    const QVector<BaseRoomNpcState> npcs = m_roomStates.at(roomRow).npcs;
    for (int index = 0; index < npcs.size(); ++index) {
        m_npcTable->insertRow(index);
        m_npcTable->setItem(index, 0, new QTableWidgetItem(npcs.at(index).nickname));

        auto *roleCombo = new QComboBox(m_npcTable);
        roleCombo->addItems(roleChoices);
        if (!npcs.at(index).role.trimmed().isEmpty() && roleCombo->findText(npcs.at(index).role.trimmed()) < 0)
            roleCombo->addItem(npcs.at(index).role.trimmed());
        roleCombo->setCurrentText(npcs.at(index).role.trimmed().isEmpty() ? roleChoices.value(0) : npcs.at(index).role.trimmed());
        connect(roleCombo, &QComboBox::currentTextChanged, this, [this]() { syncNpcStateFromTable(); });
        m_npcTable->setCellWidget(index, 1, roleCombo);
    }

    m_npcTable->blockSignals(false);
}

void BaseEditDialog::syncNpcStateFromTable()
{
    const int roomRow = m_roomTable->currentRow();
    if (roomRow < 0 || roomRow >= m_roomStates.size() || !m_npcTable)
        return;

    QVector<BaseRoomNpcState> npcs;
    for (int row = 0; row < m_npcTable->rowCount(); ++row) {
        BaseRoomNpcState npc = m_roomStates.at(roomRow).npcs.value(row);
        if (const auto *nicknameItem = m_npcTable->item(row, 0))
            npc.nickname = nicknameItem->text().trimmed();
        if (auto *roleCombo = qobject_cast<QComboBox *>(m_npcTable->cellWidget(row, 1)))
            npc.role = roleCombo->currentText().trimmed();
        if (!npc.nickname.isEmpty())
            npcs.append(npc);
    }
    m_roomStates[roomRow].npcs = npcs;
    if (m_removeNpcButton)
        m_removeNpcButton->setEnabled(!npcs.isEmpty());
}

QStringList BaseEditDialog::roleChoicesForRoom(const QString &roomName) const
{
    const QString room = roomName.trimmed().toLower();
    if (room == QStringLiteral("bar"))
        return {QStringLiteral("bartender"), QStringLiteral("BarFly"), QStringLiteral("NewsVendor")};
    if (room == QStringLiteral("trader"))
        return {QStringLiteral("trader")};
    if (room == QStringLiteral("equipment") || room == QStringLiteral("equip"))
        return {QStringLiteral("Equipment")};
    if (room == QStringLiteral("shipdealer") || room == QStringLiteral("ship_dealer") || room == QStringLiteral("ship-dealer"))
        return {QStringLiteral("ShipDealer")};
    if (room == QStringLiteral("deck"))
        return {QStringLiteral("ShipDealer"), QStringLiteral("trader"), QStringLiteral("Equipment"), QStringLiteral("bartender")};
    if (room == QStringLiteral("cityscape"))
        return {QStringLiteral("trader")};
    return {QStringLiteral("trader")};
}

void BaseEditDialog::populateSceneCombo(int row, const QString &roomName, const QString &currentScene)
{
    const BaseDialogCatalog &catalog = sharedBaseDialogCatalog();
    auto *sceneCombo = new QComboBox(m_roomTable);
    sceneCombo->setEditable(false);

    const QString roomKey = roomPreviewKind(roomName);
    const auto options = catalog.roomSceneOptionsByRoom.value(roomKey);
    for (const auto &option : options)
        sceneCombo->addItem(option.first, option.second);

    if (!currentScene.trimmed().isEmpty()) {
        bool found = false;
        for (int index = 0; index < sceneCombo->count(); ++index) {
            if (sceneCombo->itemData(index).toString().compare(currentScene.trimmed(), Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            sceneCombo->addItem(roomSceneDisplayLabel(tr("Aktueller Eintrag"), currentScene.trimmed()), currentScene.trimmed());
    }
    if (!currentScene.trimmed().isEmpty()) {
        for (int index = 0; index < sceneCombo->count(); ++index) {
            if (sceneCombo->itemData(index).toString().compare(currentScene.trimmed(), Qt::CaseInsensitive) == 0) {
                sceneCombo->setCurrentIndex(index);
                break;
            }
        }
    }

    connect(sceneCombo, &QComboBox::currentTextChanged, this, [this, row]() {
        if (row >= 0 && row < m_roomStates.size()) {
            if (auto *combo = qobject_cast<QComboBox *>(m_roomTable->cellWidget(row, 3)))
                m_roomStates[row].scenePath = comboDataOrText(combo);
        }
        if (row == selectedRoomRow())
            refreshRoomPreview();
    });
    m_roomTable->setCellWidget(row, 3, sceneCombo);
}

void BaseEditDialog::refreshPreview()
{
    if (!m_preview || !m_previewFallback || !m_previewStack || !m_archetypeCombo)
        return;

    const BaseDialogCatalog &catalog = sharedBaseDialogCatalog();
    const QString archetype = m_archetypeCombo->currentText().trimmed();
    auto showFallback = [&](const QString &message) {
        m_previewFallback->setText(message);
        m_previewStack->setCurrentWidget(m_previewFallback);
        m_preview->clearModel();
    };

    if (archetype.isEmpty()) {
        showFallback(tr("Waehle einen Archetype um die Vorschau zu laden."));
        return;
    }

    const QString modelPath = catalog.archetypeModelPaths.value(normalizedKey(archetype));
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        showFallback(tr("Kein 3D-Modell fuer %1 vorhanden.").arg(archetype));
        return;
    }

    QString errorMessage;
    if (!m_preview->loadModelFile(modelPath, &errorMessage)) {
        showFallback(tr("Modell konnte nicht geladen werden: %1")
                         .arg(errorMessage.isEmpty() ? tr("unbekannter Fehler") : errorMessage));
        return;
    }

    m_previewStack->setCurrentWidget(m_preview);
}

int BaseEditDialog::selectedRoomRow() const
{
    return findRoomRowByName(m_selectedRoomKey);
}

QString BaseEditDialog::selectedRoomName() const
{
    const int row = selectedRoomRow();
    return row >= 0 && row < m_roomStates.size() ? m_roomStates.at(row).roomName.trimmed() : QString();
}

int BaseEditDialog::activeRoomRow() const
{
    return findRoomRowByName(m_activeRoomKey);
}

QString BaseEditDialog::activeRoomName() const
{
    const int row = activeRoomRow();
    return row >= 0 && row < m_roomStates.size() ? m_roomStates.at(row).roomName.trimmed() : QString();
}

int BaseEditDialog::findRoomRowByName(const QString &roomName) const
{
    const QString target = normalizedKey(roomName);
    if (target.isEmpty())
        return -1;
    for (int row = 0; row < m_roomStates.size(); ++row) {
        if (normalizedKey(m_roomStates.at(row).roomName) == target)
            return row;
    }
    return -1;
}

void BaseEditDialog::setSelectedRoom(const QString &roomName)
{
    const int row = findRoomRowByName(roomName);
    if (row < 0 || !m_roomTable || !m_roomTable->selectionModel()) {
        populateNpcTable(-1);
        updateRoomSelectionUi();
        refreshRoomPreview();
        return;
    }

    m_selectedRoomKey = m_roomStates.at(row).roomName.trimmed();
    {
        QSignalBlocker selectionBlocker(m_roomTable->selectionModel());
        m_roomTable->selectRow(row);
    }
    populateNpcTable(row);
    updateRoomSelectionUi();
    refreshRoomPreview();
}

void BaseEditDialog::setActiveRoom(const QString &roomName)
{
    const int row = findRoomRowByName(roomName);
    if (row < 0) {
        m_activeRoomKey.clear();
        updateRoomActivationUi();
        return;
    }

    m_activeRoomKey = m_roomStates.at(row).roomName.trimmed();
    updateRoomActivationUi();
}

void BaseEditDialog::updateRoomSelectionUi()
{
    const QString roomName = selectedRoomName();
    if (m_selectedRoomLabel) {
        m_selectedRoomLabel->setText(roomName.isEmpty()
                                         ? tr("Kein Raum ausgewaehlt")
                                         : tr("Ausgewaehlter Raum: %1").arg(roomName));
    }
    if (m_activeRoomLabel)
        m_activeRoomLabel->setText(tr("Aktiver Raum im Viewer: %1").arg(activeRoomName().isEmpty() ? QStringLiteral("-") : activeRoomName()));
}

void BaseEditDialog::updateRoomActivationUi()
{
    const QString activeKey = normalizedKey(m_activeRoomKey);
    for (int row = 0; row < m_roomTable->rowCount(); ++row) {
        const QString roomName = m_roomStates.value(row).roomName.trimmed();
        const bool isActive = !activeKey.isEmpty() && normalizedKey(roomName) == activeKey;
        if (auto *button = qobject_cast<QPushButton *>(m_roomTable->cellWidget(row, 1))) {
            button->setProperty("roomRow", row);
            button->setText(isActive ? tr("Active") : tr("Activate"));
        }
        if (auto *roomItem = m_roomTable->item(row, 2)) {
            QFont font = roomItem->font();
            font.setBold(isActive);
            roomItem->setFont(font);
        }
    }
    updateRoomSelectionUi();
}

void BaseEditDialog::refreshRoomPreview()
{
    if (!m_roomPreview || !m_roomPreviewFallback || !m_roomPreviewStack)
        return;

    auto showFallback = [&](const QString &message) {
        m_roomPreviewFallback->setText(message);
        m_roomPreviewStack->setCurrentWidget(m_roomPreviewFallback);
        m_roomPreview->clearModel();
    };

    const int row = activeRoomRow();
    if (row < 0 || row >= m_roomStates.size()) {
        showFallback(tr("Aktiviere einen Raum, um die Room-Vorschau zu laden."));
        return;
    }

    const BaseRoomState room = m_roomStates.at(row);
    if (room.scenePath.trimmed().isEmpty()) {
        showFallback(tr("Der ausgewaehlte Raum hat keinen Scene-Pfad."));
        return;
    }

    const QString modelPath = resolveBaseRoomPreviewModelPath(room.roomName,
                                                              room.scenePath,
                                                              flatlas::core::EditingContext::instance().primaryGamePath());
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        showFallback(tr("Fuer %1 konnte kein Interior-Modell aufgeloest werden.")
                         .arg(room.roomName.trimmed().isEmpty() ? tr("diesen Raum") : room.roomName.trimmed()));
        return;
    }

    QString errorMessage;
    if (!m_roomPreview->loadModelFile(modelPath, &errorMessage)) {
        showFallback(tr("Room-Modell konnte nicht geladen werden: %1")
                         .arg(errorMessage.isEmpty() ? tr("unbekannter Fehler") : errorMessage));
        return;
    }

    m_roomPreviewStack->setCurrentWidget(m_roomPreview);
}

void BaseEditDialog::applyTemplateSelection()
{
    if (m_initialState.editMode || !m_templateCombo)
        return;

    const QString previousSelected = m_selectedRoomKey;
    const QString previousActive = m_activeRoomKey;
    const QString templateBase = comboStoredValue(m_templateCombo);
    if (templateBase.isEmpty()) {
        m_roomStates = BaseEditService::defaultRoomsForArchetype(m_archetypeCombo ? m_archetypeCombo->currentText().trimmed()
                                                                                  : m_initialState.archetype);
        m_templateInfoLabel->clear();
    } else {
        BaseEditState templateState;
        QString errorMessage;
        if (!BaseEditService::loadTemplateState(templateBase,
                                               flatlas::core::EditingContext::instance().primaryGamePath(),
                                               m_textOverrides,
                                               &templateState,
                                               &errorMessage)) {
            m_templateInfoLabel->setText(errorMessage.trimmed().isEmpty()
                                             ? tr("Template konnte nicht geladen werden.")
                                             : errorMessage.trimmed());
            return;
        }

        BaseEditState targetState = state();
        targetState.archetype = m_archetypeCombo ? m_archetypeCombo->currentText().trimmed() : m_initialState.archetype;
        targetState.baseNickname = m_baseNicknameEdit ? m_baseNicknameEdit->text().trimmed() : m_initialState.baseNickname;
        targetState.reputation = comboStoredValue(m_reputationCombo);
        QVector<BaseRoomState> mergedRooms = BaseEditService::applyTemplateRoomsForCreate(targetState,
                                                                                          templateState,
                                                                                          m_copyNpcsCheck->isChecked());

        QStringList infoLines;
        for (const BaseRoomState &room : templateState.rooms) {
            const QString roomName = room.roomName.trimmed();
            if (roomName.isEmpty())
                continue;
            infoLines.append(QStringLiteral("%1: %2")
                                 .arg(roomName,
                                      room.scenePath.trimmed().isEmpty() ? QStringLiteral("-") : room.scenePath.trimmed()));
        }

        m_roomStates = mergedRooms;
        m_templateInfoLabel->setText(infoLines.isEmpty()
                                         ? tr("Template enthaelt keine Raeume.")
                                         : tr("Template-Raeume:\n%1").arg(infoLines.join(QLatin1Char('\n'))));
        m_priceVarianceSpin->setValue(templateState.priceVariance);
        if (!templateState.startRoom.trimmed().isEmpty())
            m_startRoomCombo->setCurrentText(templateState.startRoom.trimmed());
        if (!templateState.bgcsBaseRunBy.trimmed().isEmpty() && m_bgcsEdit->text().trimmed().isEmpty())
            m_bgcsEdit->setText(templateState.bgcsBaseRunBy.trimmed());
    }

    populateRooms(m_roomStates);
    refreshStartRooms();

    m_selectedRoomKey = findRoomRowByName(previousSelected) >= 0 ? previousSelected : QString();
    m_activeRoomKey = findRoomRowByName(previousActive) >= 0 ? previousActive : QString();

    if (m_selectedRoomKey.isEmpty()) {
        for (const BaseRoomState &room : m_roomStates) {
            if (room.enabled && !room.roomName.trimmed().isEmpty()) {
                m_selectedRoomKey = room.roomName.trimmed();
                break;
            }
        }
    }
    if (m_selectedRoomKey.isEmpty() && !m_roomStates.isEmpty())
        m_selectedRoomKey = m_roomStates.constFirst().roomName.trimmed();
    if (m_activeRoomKey.isEmpty())
        m_activeRoomKey = m_selectedRoomKey;

    setSelectedRoom(m_selectedRoomKey);
    setActiveRoom(m_activeRoomKey);
}

void BaseEditDialog::applyArchetypeDefaults()
{
    if (m_initialState.editMode || !m_archetypeCombo)
        return;

    const QString archetype = m_archetypeCombo->currentText().trimmed();
    if (archetype.isEmpty())
        return;

    BaseArchetypeDefaults defaults = m_archetypeDefaultsCache.value(normalizedKey(archetype));
    if (defaults.archetype.isEmpty()) {
        defaults = BaseEditService::archetypeDefaults(archetype,
                                                      flatlas::core::EditingContext::instance().primaryGamePath(),
                                                      m_textOverrides);
        m_archetypeDefaultsCache.insert(normalizedKey(archetype), defaults);
    }

    const QString currentLoadout = m_loadoutCombo ? m_loadoutCombo->currentText().trimmed() : QString();
    if (m_loadoutCombo && (currentLoadout.isEmpty() || currentLoadout == m_lastSuggestedLoadout))
        m_loadoutCombo->setCurrentText(defaults.loadout.trimmed());
    m_lastSuggestedLoadout = defaults.loadout.trimmed();

    const QString suggestedInfo = defaults.infocardXml.trimmed();
    const QString currentInfo = m_infocardEdit ? m_infocardEdit->toPlainText().trimmed() : QString();
    if (m_infocardEdit && (currentInfo.isEmpty() || currentInfo == m_lastSuggestedIdsInfo))
        m_infocardEdit->setPlainText(suggestedInfo);
    m_lastSuggestedIdsInfo = suggestedInfo;
}

} // namespace flatlas::editors