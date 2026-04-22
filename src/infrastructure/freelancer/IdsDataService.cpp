#include "IdsDataService.h"

#include "core/PathUtils.h"
#include "infrastructure/freelancer/ResourceDllWriter.h"
#include "infrastructure/io/DllResources.h"
#include "infrastructure/parser/IniParser.h"
#include "infrastructure/parser/XmlInfocard.h"

#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace flatlas::infrastructure {

namespace {

QString normalizeDllName(QString value)
{
    value = QFileInfo(value.trimmed()).fileName();
    return value.toLower();
}

QString exeDirForGameRoot(const QString &gameRoot)
{
    if (gameRoot.trimmed().isEmpty())
        return {};

    const QFileInfo info(gameRoot);
    if (info.isFile() && info.fileName().compare(QStringLiteral("freelancer.ini"), Qt::CaseInsensitive) == 0)
        return info.absolutePath();

    QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    if (!exeDir.isEmpty())
        return exeDir;

    if (!flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("freelancer.ini")).isEmpty())
        return QDir(gameRoot).absolutePath();

    return {};
}

QString freelancerIniPathForGameRoot(const QString &gameRoot)
{
    const QString exeDir = exeDirForGameRoot(gameRoot);
    if (exeDir.isEmpty())
        return {};

    const QString resolved = flatlas::core::PathUtils::ciResolvePath(exeDir, QStringLiteral("freelancer.ini"));
    if (!resolved.isEmpty())
        return resolved;
    return QDir(exeDir).absoluteFilePath(QStringLiteral("freelancer.ini"));
}

QString dataDirForGameRoot(const QString &gameRoot)
{
    QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (!dataDir.isEmpty())
        return dataDir;

    const QString exeDir = exeDirForGameRoot(gameRoot);
    if (!exeDir.isEmpty()) {
        const QString parentDataDir = flatlas::core::PathUtils::ciResolvePath(QFileInfo(exeDir).absolutePath(), QStringLiteral("DATA"));
        if (!parentDataDir.isEmpty())
            return parentDataDir;
    }

    return {};
}

QString relativePathFrom(const QString &root, const QString &path)
{
    if (root.isEmpty() || path.isEmpty())
        return path;
    return QDir(root).relativeFilePath(path);
}

bool isEditableFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists())
        return false;
    if (info.isWritable())
        return true;
    return QFileInfo(info.absolutePath()).isWritable();
}

QString normalizedUsageList(const QStringList &usageTypes)
{
    QStringList copy = usageTypes;
    copy.removeDuplicates();
    copy.sort(Qt::CaseInsensitive);
    return copy.join(QStringLiteral(", "));
}

QString sanitizeInfocardXmlLike(const QString &xml)
{
    QString text = xml.trimmed();
    if (!text.startsWith(QStringLiteral("<RDL"), Qt::CaseInsensitive)
        && !text.startsWith(QStringLiteral("<?xml"), Qt::CaseInsensitive)) {
        text = QStringLiteral("<RDL>") + text + QStringLiteral("</RDL>");
    }

    static const QRegularExpression ampRe(QStringLiteral("&(?!amp;|lt;|gt;|quot;|apos;|#)"));
    text.replace(ampRe, QStringLiteral("&amp;"));

    static const QRegularExpression voidRe(
        QStringLiteral("<(PARA|JUST|TRA|PUSH|POP)\\b([^/>]*)>"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(voidRe, QStringLiteral("<\\1\\2/>"));
    return text;
}

QString searchBlobForEntry(const IdsEntryRecord &entry)
{
    return QStringLiteral("%1 %2 %3 %4 %5 %6")
        .arg(entry.globalId)
        .arg(entry.localId)
        .arg(entry.dllName)
        .arg(entry.stringValue)
        .arg(entry.plainText)
        .arg(normalizedUsageList(entry.usageTypes))
        .toLower();
}

QString sanitizeValue(QString value)
{
    value.replace(QChar::Null, QChar());
    return value.trimmed();
}

struct SectionScanState {
    QString filePath;
    QString relativePath;
    QString sectionName;
    int sectionIndex = -1;
    int sectionStartLine = 0;
    QString nickname;
    QString archetype;
    QString baseNickname;
    QString gotoTarget;
    int idsName = 0;
    int idsInfo = 0;
    int stridName = 0;
    bool hasIdsName = false;
    bool hasIdsInfo = false;
    bool hasStridName = false;
};

bool parseIntValue(const QString &value, int *out)
{
    bool ok = false;
    const int parsed = value.trimmed().toInt(&ok);
    if (!ok)
        return false;
    if (out)
        *out = parsed;
    return true;
}

struct ExpectedIdsFields {
    bool idsName = false;
    bool idsInfo = false;
};

bool hasAssignedName(const SectionScanState &state)
{
    return (state.hasIdsName && state.idsName > 0)
        || (state.hasStridName && state.stridName > 0);
}

ExpectedIdsFields expectedIdsFieldsForSection(const SectionScanState &state)
{
    const QString section = state.sectionName.trimmed().toLower();
    if (section == QStringLiteral("system"))
        return {true, true};
    if (section == QStringLiteral("base"))
        return {true, true};
    if (section == QStringLiteral("mbase"))
        return {true, true};
    if (section == QStringLiteral("room"))
        return {true, false};

    if (section == QStringLiteral("object")) {
        const bool isNamedWorldObject = !state.baseNickname.trimmed().isEmpty()
            || !state.gotoTarget.trimmed().isEmpty();
        if (isNamedWorldObject)
            return {true, true};
    }

    return {};
}

void maybeAppendMissing(const SectionScanState &state,
                        QVector<IdsMissingAssignmentRecord> *missingAssignments)
{
    if (!missingAssignments)
        return;

    const ExpectedIdsFields expected = expectedIdsFieldsForSection(state);
    if (!expected.idsName && !expected.idsInfo)
        return;

    if (expected.idsName && !hasAssignedName(state)) {
        IdsMissingAssignmentRecord record;
        record.usageType = IdsUsageType::IdsName;
        record.filePath = state.filePath;
        record.relativePath = state.relativePath;
        record.sectionName = state.sectionName;
        record.sectionIndex = state.sectionIndex;
        record.nickname = state.nickname;
        record.archetype = state.archetype;
        record.lineNumber = state.sectionStartLine;
        record.currentValue = state.idsName;
        record.siblingValue = state.idsInfo;
        missingAssignments->append(record);
    }

    if (expected.idsInfo && (!state.hasIdsInfo || state.idsInfo <= 0)) {
        IdsMissingAssignmentRecord record;
        record.usageType = IdsUsageType::IdsInfo;
        record.filePath = state.filePath;
        record.relativePath = state.relativePath;
        record.sectionName = state.sectionName;
        record.sectionIndex = state.sectionIndex;
        record.nickname = state.nickname;
        record.archetype = state.archetype;
        record.lineNumber = state.sectionStartLine;
        record.currentValue = state.idsInfo;
        record.siblingValue = state.idsName;
        missingAssignments->append(record);
    }
}

void appendReference(QVector<IdsReferenceRecord> *references,
                     const SectionScanState &state,
                     IdsUsageType usageType,
                     const QString &keyName,
                     int globalId,
                     int lineNumber,
                     const QString &lineText)
{
    if (!references || globalId <= 0)
        return;

    IdsReferenceRecord record;
    record.globalId = globalId;
    record.usageType = usageType;
    record.filePath = state.filePath;
    record.relativePath = state.relativePath;
    record.sectionName = state.sectionName;
    record.sectionIndex = state.sectionIndex;
    record.keyName = keyName;
    record.nickname = state.nickname;
    record.archetype = state.archetype;
    record.lineNumber = lineNumber;
    record.lineText = lineText.trimmed();
    references->append(record);
}

void scanIniFile(const QString &filePath,
                 const QString &dataRoot,
                 QVector<IdsReferenceRecord> *references,
                 QVector<IdsMissingAssignmentRecord> *missingAssignments)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QString text = QString::fromLocal8Bit(file.readAll());
    file.close();

    const QStringList lines = text.split(QLatin1Char('\n'));
    QRegularExpression sectionRe(QStringLiteral("^\\s*\\[([^\\]]+)\\]"));
    QRegularExpression entryRe(QStringLiteral("^\\s*([^=;#]+?)\\s*=\\s*(.*?)\\s*$"));

    SectionScanState state;
    state.filePath = filePath;
    state.relativePath = relativePathFrom(dataRoot, filePath);
    bool hasSection = false;
    int sectionCounter = -1;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        line.remove(QChar('\r'));

        const auto sectionMatch = sectionRe.match(line);
        if (sectionMatch.hasMatch()) {
            if (hasSection)
                maybeAppendMissing(state, missingAssignments);

            state = {};
            state.filePath = filePath;
            state.relativePath = relativePathFrom(dataRoot, filePath);
            state.sectionName = sanitizeValue(sectionMatch.captured(1));
            state.sectionIndex = ++sectionCounter;
            state.sectionStartLine = i + 1;
            hasSection = true;
            continue;
        }

        if (!hasSection)
            continue;

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('#')))
            continue;

        const auto entryMatch = entryRe.match(line);
        if (!entryMatch.hasMatch())
            continue;

        const QString key = sanitizeValue(entryMatch.captured(1));
        QString value = entryMatch.captured(2);
        const int commentIdx = value.indexOf(QLatin1Char(';'));
        if (commentIdx >= 0)
            value = value.left(commentIdx);
        value = sanitizeValue(value);
        const QString lowered = key.toLower();

        if (lowered == QStringLiteral("nickname"))
            state.nickname = value;
        else if (lowered == QStringLiteral("archetype"))
            state.archetype = value;
        else if (lowered == QStringLiteral("base"))
            state.baseNickname = value;
        else if (lowered == QStringLiteral("goto"))
            state.gotoTarget = value;

        int globalId = 0;
        if (lowered == QStringLiteral("ids_name")) {
            state.hasIdsName = true;
            if (parseIntValue(value, &globalId))
                state.idsName = globalId;
            appendReference(references, state, IdsUsageType::IdsName, key, globalId, i + 1, line);
        } else if (lowered == QStringLiteral("ids_info")) {
            state.hasIdsInfo = true;
            if (parseIntValue(value, &globalId))
                state.idsInfo = globalId;
            appendReference(references, state, IdsUsageType::IdsInfo, key, globalId, i + 1, line);
        } else if (lowered == QStringLiteral("strid_name")) {
            state.hasStridName = true;
            if (parseIntValue(value, &globalId))
                state.stridName = globalId;
            appendReference(references, state, IdsUsageType::StridName, key, globalId, i + 1, line);
        }
    }

    if (hasSection)
        maybeAppendMissing(state, missingAssignments);
}

void collectIdsIssues(IdsDataset *dataset)
{
    if (!dataset)
        return;

    QHash<int, int> entryCountById;
    QHash<int, const IdsEntryRecord *> entryById;
    QHash<int, QStringList> usageKindsById;
    QHash<int, int> referenceCountById;

    for (const auto &entry : std::as_const(dataset->entries)) {
        entryCountById[entry.globalId] += (entry.hasStringValue ? 1 : 0) + (entry.hasHtmlValue ? 1 : 0);
        entryById.insert(entry.globalId, &entry);
        if (entry.hasStringValue && entry.hasHtmlValue) {
            IdsIssueRecord issue;
            issue.severity = IdsIssueSeverity::Warning;
            issue.code = QStringLiteral("mixed-resource-kind");
            issue.globalId = entry.globalId;
            issue.dllName = entry.dllName;
            issue.message = QStringLiteral("ID %1 exists both as string-table and HTML resource in %2.")
                                .arg(entry.globalId)
                                .arg(entry.dllName);
            dataset->issues.append(issue);
        }

        if (entry.hasHtmlValue) {
            QString validationError;
            if (!XmlInfocard::validate(entry.htmlValue, validationError)) {
                IdsIssueRecord issue;
                issue.severity = IdsIssueSeverity::Error;
                issue.code = QStringLiteral("invalid-infocard-xml");
                issue.globalId = entry.globalId;
                issue.dllName = entry.dllName;
                issue.message = QStringLiteral("ids_info %1 in %2 has invalid XML/RDL: %3")
                                    .arg(entry.globalId)
                                    .arg(entry.dllName, validationError);
                dataset->issues.append(issue);
            }
        }
    }

    for (const auto &reference : std::as_const(dataset->references)) {
        usageKindsById[reference.globalId].append(IdsDataService::usageTypeKey(reference.usageType));
        referenceCountById[reference.globalId] += 1;
    }

    for (auto it = entryCountById.cbegin(); it != entryCountById.cend(); ++it) {
        if (it.value() > 1) {
            const IdsEntryRecord *entry = entryById.value(it.key(), nullptr);
            IdsIssueRecord issue;
            issue.severity = IdsIssueSeverity::Warning;
            issue.code = QStringLiteral("duplicate-id");
            issue.globalId = it.key();
            if (entry)
                issue.dllName = entry->dllName;
            issue.message = QStringLiteral("ID %1 is present more than once across loaded resources.")
                                .arg(it.key());
            dataset->issues.append(issue);
        }
    }

    for (auto it = usageKindsById.begin(); it != usageKindsById.end(); ++it) {
        if (!entryById.contains(it.key())) {
            IdsIssueRecord issue;
            issue.severity = IdsIssueSeverity::Error;
            issue.code = QStringLiteral("missing-referenced-id");
            issue.globalId = it.key();
            issue.message = QStringLiteral("Referenced ID %1 does not exist in any loaded DLL (%2).")
                                .arg(it.key())
                                .arg(normalizedUsageList(it.value()));
            dataset->issues.append(issue);
        }
    }

    for (const auto &entry : std::as_const(dataset->entries)) {
        if (!referenceCountById.contains(entry.globalId)) {
            IdsIssueRecord issue;
            issue.severity = IdsIssueSeverity::Info;
            issue.code = QStringLiteral("unused-id");
            issue.globalId = entry.globalId;
            issue.dllName = entry.dllName;
            issue.message = QStringLiteral("ID %1 from %2 is currently unused.")
                                .arg(entry.globalId)
                                .arg(entry.dllName);
            dataset->issues.append(issue);
        }
    }
}

IdsEntryRecord *ensureEntry(QHash<int, IdsEntryRecord> *entries, int globalId)
{
    auto it = entries->find(globalId);
    if (it == entries->end()) {
        IdsEntryRecord entry;
        entry.globalId = globalId;
        it = entries->insert(globalId, entry);
    }
    return &it.value();
}

QString resolveDllPath(const QString &exeDir, const QString &dllName)
{
    const QString resolved = flatlas::core::PathUtils::ciResolvePath(exeDir, dllName);
    if (!resolved.isEmpty())
        return resolved;
    return QDir(exeDir).absoluteFilePath(dllName);
}

int predictedSlotForDll(const IdsDataset &dataset, const QString &dllName)
{
    const QString normalizedTarget = normalizeDllName(dllName);
    if (normalizedTarget.isEmpty())
        return 0;

    if (!dataset.freelancerIniPath.isEmpty()) {
        const int slot = ResourceDllWriter::slotForDll(dataset.freelancerIniPath, dllName);
        if (slot > 0)
            return slot;
    }

    for (int index = 0; index < dataset.resourceDlls.size(); ++index) {
        if (normalizeDllName(dataset.resourceDlls.at(index)) == normalizedTarget)
            return index + 1;
    }

    for (const auto &entry : dataset.entries) {
        if (normalizeDllName(entry.dllName) == normalizedTarget && entry.dllSlot > 0)
            return entry.dllSlot;
    }

    return dataset.resourceDlls.size() + 1;
}

} // namespace

QString IdsDataService::defaultCreationDllName(const IdsDataset &dataset)
{
    if (!dataset.freelancerIniPath.isEmpty())
        return ResourceDllWriter::targetFlatlasResourceDll(dataset.freelancerIniPath);

    for (const QString &dllName : dataset.resourceDlls) {
        if (ResourceDllWriter::isFlatlasResourceDll(dllName))
            return dllName;
    }

    for (const auto &entry : dataset.entries) {
        if (ResourceDllWriter::isFlatlasResourceDll(entry.dllName))
            return entry.dllName;
    }

    if (!dataset.resourceDlls.isEmpty())
        return dataset.resourceDlls.first();
    if (!dataset.entries.isEmpty())
        return dataset.entries.first().dllName;
    return ResourceDllWriter::preferredFlatlasDllName();
}

int IdsDataService::nextAvailableGlobalId(const IdsDataset &dataset, const QString &dllName)
{
    const QString targetDll = dllName.trimmed().isEmpty() ? defaultCreationDllName(dataset) : dllName.trimmed();
    const int slot = predictedSlotForDll(dataset, targetDll);
    if (slot <= 0)
        return 0;

    QSet<int> usedLocalIds;
    const QString normalizedTarget = normalizeDllName(targetDll);
    for (const auto &entry : dataset.entries) {
        if (normalizeDllName(entry.dllName) == normalizedTarget && entry.localId > 0)
            usedLocalIds.insert(entry.localId);
    }

    int localId = 1;
    while (usedLocalIds.contains(localId))
        ++localId;

    return ResourceDllWriter::makeGlobalId(slot, localId);
}

IdsDataset IdsDataService::loadFromGameRoot(const QString &gameRoot)
{
    IdsDataset dataset;
    dataset.gameRoot = gameRoot;
    dataset.exeDir = exeDirForGameRoot(gameRoot);
    dataset.dataDir = dataDirForGameRoot(gameRoot);
    dataset.freelancerIniPath = freelancerIniPathForGameRoot(gameRoot);

    if (!dataset.freelancerIniPath.isEmpty())
        dataset.resourceDlls = ResourceDllWriter::resourceDllsFromFreelancerIni(dataset.freelancerIniPath);

    QHash<int, IdsEntryRecord> entriesById;
    QSet<QString> seenDlls;
    for (int i = 0; i < dataset.resourceDlls.size(); ++i) {
        const QString dllName = dataset.resourceDlls.at(i);
        const QString normalizedDll = normalizeDllName(dllName);
        if (seenDlls.contains(normalizedDll)) {
            IdsIssueRecord issue;
            issue.severity = IdsIssueSeverity::Warning;
            issue.code = QStringLiteral("duplicate-dll-registration");
            issue.dllName = dllName;
            issue.message = QStringLiteral("Resource DLL %1 is registered more than once in freelancer.ini.").arg(dllName);
            dataset.issues.append(issue);
            continue;
        }
        seenDlls.insert(normalizedDll);

        const int slot = i + 1;
        const QString dllPath = resolveDllPath(dataset.exeDir, dllName);
        const auto strings = DllResources::loadStrings(dllPath, 0, 65535);
        for (auto it = strings.constBegin(); it != strings.constEnd(); ++it) {
            const int globalId = ResourceDllWriter::makeGlobalId(slot, it.key());
            auto *entry = ensureEntry(&entriesById, globalId);
            entry->localId = it.key();
            entry->dllSlot = slot;
            entry->dllName = dllName;
            entry->dllPath = dllPath;
            entry->editable = isEditableFile(dllPath);
            entry->hasStringValue = true;
            entry->stringValue = sanitizeValue(it.value());
        }

        const auto htmlEntries = ResourceDllWriter::loadHtmlResources(dllPath);
        for (auto it = htmlEntries.constBegin(); it != htmlEntries.constEnd(); ++it) {
            const int globalId = ResourceDllWriter::makeGlobalId(slot, it.key());
            auto *entry = ensureEntry(&entriesById, globalId);
            entry->localId = it.key();
            entry->dllSlot = slot;
            entry->dllName = dllName;
            entry->dllPath = dllPath;
            entry->editable = isEditableFile(dllPath);
            entry->hasHtmlValue = true;
            entry->htmlValue = sanitizeValue(it.value());
            entry->plainText = sanitizeValue(XmlInfocard::parseToPlainText(entry->htmlValue));
        }
    }

    if (!dataset.dataDir.isEmpty()) {
        QDirIterator it(dataset.dataDir,
                        QStringList() << QStringLiteral("*.ini"),
                        QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext())
            scanIniFile(it.next(), dataset.dataDir, &dataset.references, &dataset.missingAssignments);
    }

    QHash<int, QSet<QString>> usageTypesById;
    for (const auto &reference : std::as_const(dataset.references))
        usageTypesById[reference.globalId].insert(usageTypeKey(reference.usageType));

    dataset.entries = entriesById.values().toVector();
    std::sort(dataset.entries.begin(), dataset.entries.end(), [](const IdsEntryRecord &lhs, const IdsEntryRecord &rhs) {
        return lhs.globalId < rhs.globalId;
    });

    for (auto &entry : dataset.entries) {
        const auto usageTypes = usageTypesById.value(entry.globalId);
        entry.usageTypes = usageTypes.values();
        entry.usageTypes.sort(Qt::CaseInsensitive);
        entry.searchBlob = searchBlobForEntry(entry);
    }

    collectIdsIssues(&dataset);
    return dataset;
}

IdsDataset IdsDataService::loadFromDllPath(const QString &dllPath)
{
    IdsDataset dataset;
    QFileInfo info(dllPath);
    dataset.exeDir = info.absolutePath();

    const QString dllName = info.fileName();
    IdsEntryRecord entryTemplate;
    entryTemplate.dllName = dllName;
    entryTemplate.dllPath = dllPath;
    entryTemplate.dllSlot = 1;
    entryTemplate.editable = isEditableFile(dllPath);

    const auto strings = DllResources::loadStrings(dllPath, 0, 65535);
    for (auto it = strings.constBegin(); it != strings.constEnd(); ++it) {
        IdsEntryRecord entry = entryTemplate;
        entry.localId = it.key();
        entry.globalId = it.key();
        entry.hasStringValue = true;
        entry.stringValue = sanitizeValue(it.value());
        entry.searchBlob = searchBlobForEntry(entry);
        dataset.entries.append(entry);
    }

    const auto htmlEntries = ResourceDllWriter::loadHtmlResources(dllPath);
    for (auto it = htmlEntries.constBegin(); it != htmlEntries.constEnd(); ++it) {
        IdsEntryRecord entry = entryTemplate;
        entry.localId = it.key();
        entry.globalId = it.key();
        entry.hasHtmlValue = true;
        entry.htmlValue = sanitizeValue(it.value());
        entry.plainText = sanitizeValue(XmlInfocard::parseToPlainText(entry.htmlValue));
        entry.searchBlob = searchBlobForEntry(entry);
        dataset.entries.append(entry);
    }

    std::sort(dataset.entries.begin(), dataset.entries.end(), [](const IdsEntryRecord &lhs, const IdsEntryRecord &rhs) {
        return lhs.globalId < rhs.globalId;
    });
    return dataset;
}

bool IdsDataService::writeStringEntry(const IdsDataset &dataset,
                                      const QString &dllName,
                                      int currentGlobalId,
                                      const QString &text,
                                      int *outGlobalId,
                                      QString *errorMessage)
{
    return ResourceDllWriter::ensureStringResource(dataset.freelancerIniPath,
                                                   dllName,
                                                   currentGlobalId,
                                                   text,
                                                   outGlobalId,
                                                   errorMessage);
}

bool IdsDataService::writeInfocardEntry(const IdsDataset &dataset,
                                        const QString &dllName,
                                        int currentGlobalId,
                                        const QString &xmlText,
                                        int *outGlobalId,
                                        QString *errorMessage)
{
    return ResourceDllWriter::ensureHtmlResource(dataset.freelancerIniPath,
                                                 dllName,
                                                 currentGlobalId,
                                                 xmlText,
                                                 outGlobalId,
                                                 errorMessage);
}

bool IdsDataService::assignFieldValue(const QString &filePath,
                                      int sectionIndex,
                                      const QString &fieldName,
                                      int value,
                                      QString *errorMessage)
{
    IniDocument doc = IniParser::parseFile(filePath);
    if (sectionIndex < 0 || sectionIndex >= doc.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Section index %1 is out of range for %2.").arg(sectionIndex).arg(filePath);
        return false;
    }

    IniSection &section = doc[sectionIndex];
    bool updated = false;
    for (auto &entry : section.entries) {
        if (entry.first.compare(fieldName, Qt::CaseInsensitive) == 0) {
            entry.second = QString::number(value);
            updated = true;
            break;
        }
    }
    if (!updated)
        section.entries.append({fieldName, QString::number(value)});

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not open %1 for writing.").arg(filePath);
        return false;
    }

    const QString serialized = IniParser::serialize(doc);
    file.write(serialized.toUtf8());
    file.close();
    return true;
}

QString IdsDataService::usageTypeLabel(IdsUsageType usageType)
{
    switch (usageType) {
    case IdsUsageType::IdsName:
        return QStringLiteral("ids_name");
    case IdsUsageType::IdsInfo:
        return QStringLiteral("ids_info");
    case IdsUsageType::StridName:
        return QStringLiteral("strid_name");
    case IdsUsageType::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

QString IdsDataService::usageTypeKey(IdsUsageType usageType)
{
    return usageTypeLabel(usageType).toLower();
}

QString IdsDataService::normalizedEntryTypeKey(const IdsEntryRecord &entry)
{
    if (entry.hasHtmlValue)
        return QStringLiteral("ids_info");
    if (entry.usageTypes.contains(QStringLiteral("strid_name")))
        return QStringLiteral("strid_name");
    if (entry.usageTypes.contains(QStringLiteral("ids_name")))
        return QStringLiteral("ids_name");
    return QStringLiteral("string");
}

QString IdsDataService::prettyInfocardXml(const QString &xmlText, QString *errorMessage)
{
    QString validationError;
    if (!XmlInfocard::validate(xmlText, validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return {};
    }

    QDomDocument doc;
    const QString sanitized = sanitizeInfocardXmlLike(xmlText);
    const auto parseResult = doc.setContent(sanitized);
    if (!parseResult) {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1 (line %2, column %3)")
                                .arg(parseResult.errorMessage)
                                .arg(parseResult.errorLine)
                                .arg(parseResult.errorColumn);
        return {};
    }

    QString pretty = doc.toString(2).trimmed();
    if (!pretty.startsWith(QStringLiteral("<?xml")))
        pretty.prepend(QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"));
    if (errorMessage)
        errorMessage->clear();
    return pretty;
}

} // namespace flatlas::infrastructure