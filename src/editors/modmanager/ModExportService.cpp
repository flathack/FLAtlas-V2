#include "ModExportService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>
#include <QXmlStreamWriter>

#include <algorithm>

namespace flatlas::editors {
namespace {

constexpr quint32 kCrc32Polynomial = 0xedb88320U;

QByteArray hashFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return hash.result().toHex();
}

quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xffffffffU;
    for (uchar byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1U) ? kCrc32Polynomial : 0U);
    }
    return crc ^ 0xffffffffU;
}

void appendLe16(QByteArray *out, quint16 value)
{
    out->append(char(value & 0xff));
    out->append(char((value >> 8) & 0xff));
}

void appendLe32(QByteArray *out, quint32 value)
{
    out->append(char(value & 0xff));
    out->append(char((value >> 8) & 0xff));
    out->append(char((value >> 16) & 0xff));
    out->append(char((value >> 24) & 0xff));
}

struct ZipEntry {
    QString name;
    QByteArray data;
};

bool writeStoredZip(const QString &targetPath, const QVector<ZipEntry> &entries, QString *errorMessage)
{
    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Target file could not be opened: %1").arg(targetPath);
        return false;
    }

    QByteArray centralDirectory;
    quint32 offset = 0;
    const QDateTime now = QDateTime::currentDateTime();
    const QDate date = now.date();
    const QTime time = now.time();
    const quint16 dosTime = quint16((time.hour() << 11) | (time.minute() << 5) | (time.second() / 2));
    const quint16 dosDate = quint16(((date.year() - 1980) << 9) | (date.month() << 5) | date.day());

    for (const ZipEntry &entry : entries) {
        const QByteArray name = ModExportService::normalizeArchivePath(entry.name).toUtf8();
        if (name.isEmpty())
            continue;
        const quint32 entryCrc = crc32(entry.data);
        const quint32 size = static_cast<quint32>(entry.data.size());

        QByteArray local;
        appendLe32(&local, 0x04034b50U);
        appendLe16(&local, 20);
        appendLe16(&local, 0x0800); // UTF-8 names
        appendLe16(&local, 0);      // stored
        appendLe16(&local, dosTime);
        appendLe16(&local, dosDate);
        appendLe32(&local, entryCrc);
        appendLe32(&local, size);
        appendLe32(&local, size);
        appendLe16(&local, static_cast<quint16>(name.size()));
        appendLe16(&local, 0);
        local.append(name);
        file.write(local);
        file.write(entry.data);

        QByteArray central;
        appendLe32(&central, 0x02014b50U);
        appendLe16(&central, 20);
        appendLe16(&central, 20);
        appendLe16(&central, 0x0800);
        appendLe16(&central, 0);
        appendLe16(&central, dosTime);
        appendLe16(&central, dosDate);
        appendLe32(&central, entryCrc);
        appendLe32(&central, size);
        appendLe32(&central, size);
        appendLe16(&central, static_cast<quint16>(name.size()));
        appendLe16(&central, 0);
        appendLe16(&central, 0);
        appendLe16(&central, 0);
        appendLe16(&central, 0);
        appendLe32(&central, 0);
        appendLe32(&central, offset);
        central.append(name);
        centralDirectory.append(central);

        offset += static_cast<quint32>(local.size() + entry.data.size());
    }

    const quint32 centralOffset = offset;
    file.write(centralDirectory);

    QByteArray end;
    appendLe32(&end, 0x06054b50U);
    appendLe16(&end, 0);
    appendLe16(&end, 0);
    appendLe16(&end, static_cast<quint16>(entries.size()));
    appendLe16(&end, static_cast<quint16>(entries.size()));
    appendLe32(&end, static_cast<quint32>(centralDirectory.size()));
    appendLe32(&end, centralOffset);
    appendLe16(&end, 0);
    file.write(end);

    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QObject::tr("ZIP file could not be written: %1").arg(targetPath);
        return false;
    }
    return true;
}

QStringList iterFiles(const QString &root)
{
    QStringList files;
    QDir dir(root);
    if (!dir.exists())
        return files;

    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString rel = ModExportService::normalizeArchivePath(dir.relativeFilePath(path));
        if (ModExportService::isAutomaticallyExcluded(rel))
            continue;
        files.append(path);
    }

    std::sort(files.begin(), files.end(), [root](const QString &left, const QString &right) {
        const QDir dir(root);
        return ModExportService::normalizeArchivePath(dir.relativeFilePath(left))
            .compare(ModExportService::normalizeArchivePath(dir.relativeFilePath(right)), Qt::CaseInsensitive) < 0;
    });
    return files;
}

QByteArray fileBytes(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QObject::tr("File could not be read: %1").arg(path);
        return {};
    }
    return file.readAll();
}

QByteArray exportManifest(const ModExportPlan &plan, const QString &format)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), format);
    root.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("mod_root"), plan.modRoot);
    root.insert(QStringLiteral("reference_root"), plan.referenceRoot);
    root.insert(QStringLiteral("new_count"), plan.newCount());
    root.insert(QStringLiteral("modified_count"), plan.modifiedCount());
    root.insert(QStringLiteral("unchanged_count"), plan.unchangedCount);

    QJsonArray files;
    for (const ModExportFile &item : plan.exportFiles()) {
        QJsonObject row;
        row.insert(QStringLiteral("path"), item.relativePath);
        row.insert(QStringLiteral("status"), item.status);
        row.insert(QStringLiteral("size"), item.size);
        row.insert(QStringLiteral("sha256"), item.sha256);
        row.insert(QStringLiteral("reference_sha256"), item.referenceSha256);
        files.append(row);
    }
    root.insert(QStringLiteral("files"), files);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace

QVector<ModExportFile> ModExportPlan::exportFiles() const
{
    QVector<ModExportFile> out;
    for (const ModExportFile &file : files) {
        if (file.status == QStringLiteral("new") || file.status == QStringLiteral("modified"))
            out.append(file);
    }
    return out;
}

int ModExportPlan::newCount() const
{
    int count = 0;
    for (const ModExportFile &file : files) {
        if (file.status == QStringLiteral("new"))
            ++count;
    }
    return count;
}

int ModExportPlan::modifiedCount() const
{
    int count = 0;
    for (const ModExportFile &file : files) {
        if (file.status == QStringLiteral("modified"))
            ++count;
    }
    return count;
}

QString ModExportService::normalizeArchivePath(const QString &path)
{
    QString normalized = QDir::fromNativeSeparators(path.trimmed());
    QStringList parts;
    for (const QString &part : normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part == QLatin1String("."))
            continue;
        parts.append(part);
    }
    return parts.join(QLatin1Char('/'));
}

QStringList ModExportService::automaticExclusionLabels()
{
    return {
        QStringLiteral(".flatlas/"),
        QStringLiteral(".FLAtlasLauncher/"),
        QStringLiteral("FLAtlas-Change.log"),
        QStringLiteral("ReShade.log"),
    };
}

bool ModExportService::isAutomaticallyExcluded(const QString &relativePath)
{
    const QString normalized = normalizeArchivePath(relativePath).toLower();
    if (normalized.isEmpty())
        return true;
    if (normalized == QStringLiteral("flatlas-change.log") || normalized == QStringLiteral("reshade.log"))
        return true;
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.contains(QStringLiteral(".flatlas"))
        || parts.contains(QStringLiteral(".flatlaslauncher"));
}

ModExportPlan ModExportService::collectChangedFiles(const QString &modRoot,
                                                    const QString &referenceRoot,
                                                    const ProgressCallback &progress)
{
    ModExportPlan plan;
    plan.modRoot = QDir::cleanPath(modRoot);
    plan.referenceRoot = QDir::cleanPath(referenceRoot);

    QDir modDir(plan.modRoot);
    QDir referenceDir(plan.referenceRoot);
    if (!modDir.exists()) {
        plan.errors.append(QObject::tr("Mod source not found: %1").arg(plan.modRoot));
        return plan;
    }
    if (!referenceDir.exists()) {
        plan.errors.append(QObject::tr("Reference installation not found: %1").arg(plan.referenceRoot));
        return plan;
    }

    const QStringList referenceFiles = iterFiles(plan.referenceRoot);
    const QStringList modFiles = iterFiles(plan.modRoot);
    const int total = std::max(1, static_cast<int>(referenceFiles.size() + modFiles.size()));
    QHash<QString, QString> referenceByRelativePath;

    int current = 0;
    for (const QString &path : referenceFiles) {
        ++current;
        if (progress && !progress(QStringLiteral("reference"), current, total, path)) {
            plan.errors.append(QObject::tr("Scan abgebrochen."));
            return plan;
        }
        const QString key = normalizeArchivePath(referenceDir.relativeFilePath(path)).toLower();
        if (!key.isEmpty() && !referenceByRelativePath.contains(key))
            referenceByRelativePath.insert(key, path);
    }

    for (const QString &sourcePath : modFiles) {
        ++current;
        if (progress && !progress(QStringLiteral("mod"), current, total, sourcePath)) {
            plan.errors.append(QObject::tr("Scan abgebrochen."));
            return plan;
        }

        const QString relativePath = normalizeArchivePath(modDir.relativeFilePath(sourcePath));
        const QString key = relativePath.toLower();
        const QString sourceHash = QString::fromLatin1(hashFile(sourcePath));
        if (sourceHash.isEmpty()) {
            plan.errors.append(QObject::tr("File could not be read: %1").arg(sourcePath));
            continue;
        }

        const QString referencePath = referenceByRelativePath.value(key);
        QString status;
        QString referenceHash;
        if (referencePath.isEmpty()) {
            status = QStringLiteral("new");
        } else {
            referenceHash = QString::fromLatin1(hashFile(referencePath));
            if (sourceHash == referenceHash) {
                ++plan.unchangedCount;
                continue;
            }
            status = QStringLiteral("modified");
        }

        ModExportFile file;
        file.sourcePath = sourcePath;
        file.relativePath = relativePath;
        file.status = status;
        file.size = QFileInfo(sourcePath).size();
        file.sha256 = sourceHash;
        file.referenceSha256 = referenceHash;
        plan.files.append(file);
    }

    return plan;
}

ModExportPlan ModExportService::filterPlan(const ModExportPlan &plan, const QSet<QString> &excludedRelativePaths)
{
    if (excludedRelativePaths.isEmpty())
        return plan;

    QSet<QString> normalized;
    for (const QString &path : excludedRelativePaths)
        normalized.insert(normalizeArchivePath(path).toLower());

    ModExportPlan filtered = plan;
    filtered.files.clear();
    for (const ModExportFile &file : plan.files) {
        if (!normalized.contains(file.relativePath.toLower()))
            filtered.files.append(file);
    }
    return filtered;
}

QString ModExportService::defaultScriptXml(const QString &name,
                                           const QString &author,
                                           const QString &description,
                                           bool saveSafe)
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("script"));
    xml.writeStartElement(QStringLiteral("header"));
    xml.writeAttribute(QStringLiteral("name"), name.trimmed().isEmpty() ? QStringLiteral("FLAtlas Export") : name.trimmed());
    xml.writeAttribute(QStringLiteral("savesafe"), saveSafe ? QStringLiteral("true") : QStringLiteral("false"));
    xml.writeTextElement(QStringLiteral("scriptversion"), QStringLiteral("2"));
    xml.writeTextElement(QStringLiteral("author"), author);
    xml.writeTextElement(QStringLiteral("description"), description);
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return out;
}

bool ModExportService::writeZip(const ModExportPlan &plan,
                                const QString &targetPath,
                                QString *errorMessage,
                                const ProgressCallback &progress)
{
    QVector<ZipEntry> entries;
    const auto exportFiles = plan.exportFiles();
    const int total = std::max(1, static_cast<int>(exportFiles.size()) + 1);
    int current = 0;
    for (const ModExportFile &file : exportFiles) {
        ++current;
        if (progress && !progress(QStringLiteral("file"), current, total, file.relativePath))
            return false;
        QString readError;
        const QByteArray bytes = fileBytes(file.sourcePath, &readError);
        if (!readError.isEmpty()) {
            if (errorMessage)
                *errorMessage = readError;
            return false;
        }
        entries.append({file.relativePath, bytes});
    }
    if (progress)
        progress(QStringLiteral("manifest"), total, total, QStringLiteral("FLAtlas-export-manifest.json"));
    entries.append({QStringLiteral("FLAtlas-export-manifest.json"), exportManifest(plan, QStringLiteral("zip"))});
    return writeStoredZip(targetPath, entries, errorMessage);
}

bool ModExportService::writeFlmod(const ModExportPlan &plan,
                                  const QString &targetPath,
                                  const QString &scriptXml,
                                  QString *errorMessage,
                                  const ProgressCallback &progress)
{
    QVector<ZipEntry> entries;
    entries.append({QStringLiteral("script.xml"), scriptXml.toUtf8()});

    const auto exportFiles = plan.exportFiles();
    const int total = std::max(1, static_cast<int>(exportFiles.size()) + 1);
    int current = 1;
    if (progress && !progress(QStringLiteral("script"), current, total, QStringLiteral("script.xml")))
        return false;
    for (const ModExportFile &file : exportFiles) {
        if (file.relativePath.compare(QStringLiteral("script.xml"), Qt::CaseInsensitive) == 0)
            continue;
        ++current;
        if (progress && !progress(QStringLiteral("file"), current, total, file.relativePath))
            return false;
        QString readError;
        const QByteArray bytes = fileBytes(file.sourcePath, &readError);
        if (!readError.isEmpty()) {
            if (errorMessage)
                *errorMessage = readError;
            return false;
        }
        entries.append({file.relativePath, bytes});
    }
    return writeStoredZip(targetPath, entries, errorMessage);
}

} // namespace flatlas::editors
