#include "ResourceDllWriter.h"

#include "core/PathUtils.h"
#include "infrastructure/io/DllResources.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace flatlas::infrastructure {

namespace {

QString normalizedDllName(const QString &dllName)
{
    return dllName.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/')).toLower();
}

QString dllBaseName(const QString &dllName)
{
    return QFileInfo(dllName.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'))).fileName();
}

QString resolveDllPath(const QString &freelancerIniPath, const QString &dllName)
{
    const QString exeDir = QFileInfo(freelancerIniPath).absolutePath();
    const QString resolved = flatlas::core::PathUtils::ciResolvePath(exeDir, dllName);
    if (!resolved.isEmpty())
        return resolved;
    return QDir(exeDir).absoluteFilePath(dllName);
}

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QByteArray raw = file.readAll();
    QString decoded = QString::fromUtf8(raw);
    if (!decoded.isEmpty())
        return decoded;
    return QString::fromLocal8Bit(raw);
}

bool writeText(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    file.write(text.toUtf8());
    return true;
}

void insertResourceDllLine(QString &text, const QString &dllName)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    QStringList out;
    out.reserve(lines.size() + 2);

    bool inResources = false;
    bool inserted = false;
    int lastDllIndexInOut = -1;

    for (const QString &rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            if (inResources && !inserted) {
                if (lastDllIndexInOut >= 0)
                    out.insert(lastDllIndexInOut + 1, QStringLiteral("DLL = %1").arg(dllName));
                else
                    out.append(QStringLiteral("DLL = %1").arg(dllName));
                inserted = true;
            }
            inResources = trimmed.mid(1, trimmed.size() - 2).trimmed().compare(QStringLiteral("Resources"), Qt::CaseInsensitive) == 0;
        }

        out.append(rawLine);

        if (inResources && trimmed.contains(QLatin1Char('='))) {
            const QString key = trimmed.section(QLatin1Char('='), 0, 0).trimmed();
            if (key.compare(QStringLiteral("DLL"), Qt::CaseInsensitive) == 0)
                lastDllIndexInOut = out.size() - 1;
        }
    }

    if (!inserted) {
        if (inResources) {
            if (lastDllIndexInOut >= 0)
                out.insert(lastDllIndexInOut + 1, QStringLiteral("DLL = %1").arg(dllName));
            else
                out.append(QStringLiteral("DLL = %1").arg(dllName));
        } else {
            if (!out.isEmpty() && !out.last().isEmpty())
                out.append(QString());
            out.append(QStringLiteral("[Resources]"));
            out.append(QStringLiteral("DLL = %1").arg(dllName));
        }
    }

    text = out.join(QLatin1Char('\n'));
    if (!text.endsWith(QLatin1Char('\n')))
        text.append(QLatin1Char('\n'));
}

QString templateResourceDllPath(const QString &freelancerIniPath)
{
    const QString exeDir = QFileInfo(freelancerIniPath).absolutePath();
    QString preferred = flatlas::core::PathUtils::ciResolvePath(exeDir, QStringLiteral("NameResources.dll"));
    if (!preferred.isEmpty())
        return preferred;

    const QStringList dlls = ResourceDllWriter::resourceDllsFromFreelancerIni(freelancerIniPath);
    for (const QString &dll : dlls) {
        const QString path = resolveDllPath(freelancerIniPath, dll);
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

bool ensureWritableDllExists(const QString &freelancerIniPath,
                             const QString &dllName,
                             QString *errorMessage)
{
    const QString targetPath = resolveDllPath(freelancerIniPath, dllName);
    if (QFileInfo::exists(targetPath))
        return true;

    const QString templatePath = templateResourceDllPath(freelancerIniPath);
    if (templatePath.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Es wurde keine Vorlagen-DLL zum Anlegen von %1 gefunden.").arg(dllName);
        return false;
    }

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    if (!QFile::copy(templatePath, targetPath)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Die Resource-DLL konnte nicht erstellt werden:\n%1").arg(targetPath);
        return false;
    }
    return true;
}

#ifdef Q_OS_WIN
QByteArray encodeStringTableBlock(const QMap<int, QString> &stringsByLocalId, int blockId)
{
    QByteArray blob;
    const int baseId = (blockId - 1) * 16;
    for (int i = 0; i < 16; ++i) {
        const QString text = stringsByLocalId.value(baseId + i);
        const quint16 length = static_cast<quint16>(text.size());
        blob.append(reinterpret_cast<const char *>(&length), static_cast<int>(sizeof(length)));
        if (length > 0)
            blob.append(reinterpret_cast<const char *>(text.utf16()),
                        length * static_cast<int>(sizeof(char16_t)));
    }
    return blob;
}
#endif

} // namespace

QString ResourceDllWriter::preferredFlatlasDllName()
{
    return QStringLiteral("FLAtlas_resources.dll");
}

bool ResourceDllWriter::isFlatlasResourceDll(const QString &dllName)
{
    const QString base = dllBaseName(dllName).trimmed();
    return base.compare(preferredFlatlasDllName(), Qt::CaseInsensitive) == 0;
}

QStringList ResourceDllWriter::resourceDllsFromFreelancerIni(const QString &freelancerIniPath)
{
    QStringList dlls;
    const IniDocument ini = IniParser::parseFile(freelancerIniPath);
    for (const IniSection &section : ini) {
        if (section.name.compare(QStringLiteral("Resources"), Qt::CaseInsensitive) != 0)
            continue;
        for (const QString &value : section.values(QStringLiteral("DLL"))) {
            const QString dllName = value.section(QLatin1Char(','), 0, 0).trimmed();
            if (dllName.isEmpty())
                continue;
            bool exists = false;
            for (const QString &existing : dlls) {
                if (normalizedDllName(existing) == normalizedDllName(dllName)) {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                dlls.append(dllName);
        }
        break;
    }
    return dlls;
}

QString ResourceDllWriter::findExistingFlatlasResourceDll(const QString &freelancerIniPath)
{
    const QStringList dlls = resourceDllsFromFreelancerIni(freelancerIniPath);
    for (const QString &dll : dlls) {
        if (dllBaseName(dll).compare(preferredFlatlasDllName(), Qt::CaseInsensitive) == 0)
            return dll;
    }
    return {};
}

QString ResourceDllWriter::targetFlatlasResourceDll(const QString &freelancerIniPath)
{
    const QString existing = findExistingFlatlasResourceDll(freelancerIniPath);
    if (!existing.isEmpty())
        return existing;
    return preferredFlatlasDllName();
}

int ResourceDllWriter::slotForDll(const QString &freelancerIniPath, const QString &dllName)
{
    const QString target = normalizedDllName(dllName);
    const QStringList dlls = resourceDllsFromFreelancerIni(freelancerIniPath);
    for (int i = 0; i < dlls.size(); ++i) {
        if (normalizedDllName(dlls[i]) == target)
            return i + 1;
    }
    return 0;
}

int ResourceDllWriter::makeGlobalId(int slot, int localId)
{
    return ((slot & 0xFFFF) << 16) | (localId & 0xFFFF);
}

bool ResourceDllWriter::ensureResourceDllRegistered(const QString &freelancerIniPath,
                                                    const QString &dllName,
                                                    QString *errorMessage)
{
    QString text = readText(freelancerIniPath);
    if (text.isEmpty() && !QFileInfo::exists(freelancerIniPath)) {
        if (errorMessage)
            *errorMessage = QObject::tr("freelancer.ini wurde nicht gefunden:\n%1").arg(freelancerIniPath);
        return false;
    }

    const QString target = normalizedDllName(dllName);
    const QStringList current = resourceDllsFromFreelancerIni(freelancerIniPath);
    for (const QString &dll : current) {
        if (normalizedDllName(dll) == target)
            return true;
    }

    insertResourceDllLine(text, dllName);
    if (!writeText(freelancerIniPath, text)) {
        if (errorMessage)
            *errorMessage = QObject::tr("freelancer.ini konnte nicht aktualisiert werden:\n%1").arg(freelancerIniPath);
        return false;
    }
    return true;
}

bool ResourceDllWriter::ensureFlatlasResourceDll(const QString &freelancerIniPath,
                                                 QString *outDllName,
                                                 QString *errorMessage)
{
    const QString dllName = targetFlatlasResourceDll(freelancerIniPath);
    if (!ensureResourceDllRegistered(freelancerIniPath, dllName, errorMessage))
        return false;
    if (!ensureWritableDllExists(freelancerIniPath, dllName, errorMessage))
        return false;
    if (outDllName)
        *outDllName = dllName;
    return true;
}

bool ResourceDllWriter::ensureStringResource(const QString &freelancerIniPath,
                                             const QString &dllName,
                                             int currentGlobalId,
                                             const QString &text,
                                             int *outGlobalId,
                                             QString *errorMessage)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Es wurde kein Text für strid_name angegeben.");
        return false;
    }

    QString targetDllName = dllName.trimmed();
    if (targetDllName.isEmpty() || isFlatlasResourceDll(targetDllName)) {
        if (!ensureFlatlasResourceDll(freelancerIniPath, &targetDllName, errorMessage))
            return false;
    } else {
        if (!ensureResourceDllRegistered(freelancerIniPath, targetDllName, errorMessage))
            return false;
        if (!ensureWritableDllExists(freelancerIniPath, targetDllName, errorMessage))
            return false;
    }

    const int slot = slotForDll(freelancerIniPath, targetDllName);
    if (slot <= 0) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der DLL-Slot für %1 konnte nicht bestimmt werden.").arg(targetDllName);
        return false;
    }

    const QString dllPath = resolveDllPath(freelancerIniPath, targetDllName);
    QMap<int, QString> localStrings = DllResources::loadStrings(dllPath, 0, 65535);

    int localId = 0;
    const int currentSlot = (currentGlobalId >> 16) & 0xFFFF;
    const int currentLocal = currentGlobalId & 0xFFFF;
    if (currentGlobalId > 0 && currentSlot == slot && currentLocal > 0)
        localId = currentLocal;

    if (localId <= 0) {
        localId = 1;
        while (localStrings.contains(localId))
            ++localId;
    }

    localStrings.insert(localId, trimmedText);

#ifdef Q_OS_WIN
    const int blockId = (localId / 16) + 1;
    const QByteArray data = encodeStringTableBlock(localStrings, blockId);
    HANDLE handle = BeginUpdateResourceW(reinterpret_cast<LPCWSTR>(dllPath.utf16()), FALSE);
    if (!handle) {
        if (errorMessage)
            *errorMessage = QObject::tr("Die Resource-DLL konnte nicht zum Schreiben geöffnet werden:\n%1").arg(dllPath);
        return false;
    }

    const WORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    const BOOL ok = UpdateResourceW(handle,
                                    RT_STRING,
                                    MAKEINTRESOURCEW(blockId),
                                    langId,
                                    const_cast<char *>(data.constData()),
                                    static_cast<DWORD>(data.size()));
    if (!ok) {
        EndUpdateResourceW(handle, TRUE);
        if (errorMessage)
            *errorMessage = QObject::tr("Der String-Eintrag konnte nicht in %1 geschrieben werden.").arg(dllPath);
        return false;
    }

    if (!EndUpdateResourceW(handle, FALSE)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Die Änderungen an %1 konnten nicht gespeichert werden.").arg(dllPath);
        return false;
    }
#else
    if (errorMessage)
        *errorMessage = QObject::tr("Das Schreiben von Resource-DLLs wird auf dieser Plattform nicht unterstützt.");
    return false;
#endif

    if (outGlobalId)
        *outGlobalId = makeGlobalId(slot, localId);
    return true;
}

QMap<int, QString> ResourceDllWriter::loadHtmlResources(const QString &dllPath)
{
    QMap<int, QString> result;
#ifdef Q_OS_WIN
    HMODULE handle = LoadLibraryExW(reinterpret_cast<LPCWSTR>(dllPath.utf16()),
                                    nullptr,
                                    LOAD_LIBRARY_AS_DATAFILE);
    if (!handle)
        return result;

    EnumResourceNamesW(
        handle,
        RT_HTML,
        [](HMODULE module, LPCWSTR, LPWSTR name, LONG_PTR lParam) -> BOOL {
            auto *map = reinterpret_cast<QMap<int, QString> *>(lParam);
            if (!IS_INTRESOURCE(name))
                return TRUE;

            const int localId = static_cast<int>(reinterpret_cast<quintptr>(name));
            HRSRC res = FindResourceW(module, name, RT_HTML);
            if (!res)
                return TRUE;
            HGLOBAL data = LoadResource(module, res);
            if (!data)
                return TRUE;
            const DWORD size = SizeofResource(module, res);
            const void *raw = LockResource(data);
            if (!raw || size == 0)
                return TRUE;

            QString text = QString::fromUtf8(static_cast<const char *>(raw), static_cast<int>(size));
            if (text.isEmpty())
                text = QString::fromLocal8Bit(static_cast<const char *>(raw), static_cast<int>(size));
            if (!text.trimmed().isEmpty())
                map->insert(localId, text);
            return TRUE;
        },
        reinterpret_cast<LONG_PTR>(&result));

    FreeLibrary(handle);
#else
    Q_UNUSED(dllPath);
#endif
    return result;
}

bool ResourceDllWriter::ensureHtmlResource(const QString &freelancerIniPath,
                                           const QString &dllName,
                                           int currentGlobalId,
                                           const QString &xmlText,
                                           int *outGlobalId,
                                           QString *errorMessage)
{
    const QString trimmedXml = xmlText.trimmed();
    if (trimmedXml.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Es wurde keine Infocard angegeben.");
        return false;
    }

    QString targetDllName = dllName.trimmed();
    if (targetDllName.isEmpty() || isFlatlasResourceDll(targetDllName)) {
        if (!ensureFlatlasResourceDll(freelancerIniPath, &targetDllName, errorMessage))
            return false;
    } else {
        if (!ensureResourceDllRegistered(freelancerIniPath, targetDllName, errorMessage))
            return false;
        if (!ensureWritableDllExists(freelancerIniPath, targetDllName, errorMessage))
            return false;
    }

    const int slot = slotForDll(freelancerIniPath, targetDllName);
    if (slot <= 0) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der DLL-Slot für %1 konnte nicht bestimmt werden.").arg(targetDllName);
        return false;
    }

    const QString dllPath = resolveDllPath(freelancerIniPath, targetDllName);
    const QMap<int, QString> localStrings = DllResources::loadStrings(dllPath, 0, 65535);
    QMap<int, QString> localHtml = loadHtmlResources(dllPath);

    int localId = 0;
    const int currentSlot = (currentGlobalId >> 16) & 0xFFFF;
    const int currentLocal = currentGlobalId & 0xFFFF;
    if (currentGlobalId > 0 && currentSlot == slot && currentLocal > 0)
        localId = currentLocal;

    if (localId <= 0) {
        localId = 1;
        while (localStrings.contains(localId) || localHtml.contains(localId))
            ++localId;
    }

#ifdef Q_OS_WIN
    const QByteArray data = trimmedXml.toUtf8();
    HANDLE handle = BeginUpdateResourceW(reinterpret_cast<LPCWSTR>(dllPath.utf16()), FALSE);
    if (!handle) {
        if (errorMessage)
            *errorMessage = QObject::tr("Die Resource-DLL konnte nicht zum Schreiben geöffnet werden:\n%1").arg(dllPath);
        return false;
    }

    const WORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    const BOOL ok = UpdateResourceW(handle,
                                    RT_HTML,
                                    MAKEINTRESOURCEW(localId),
                                    langId,
                                    const_cast<char *>(data.constData()),
                                    static_cast<DWORD>(data.size()));
    if (!ok) {
        EndUpdateResourceW(handle, TRUE);
        if (errorMessage)
            *errorMessage = QObject::tr("Der Infocard-Eintrag konnte nicht in %1 geschrieben werden.").arg(dllPath);
        return false;
    }

    if (!EndUpdateResourceW(handle, FALSE)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Die Änderungen an %1 konnten nicht gespeichert werden.").arg(dllPath);
        return false;
    }
#else
    if (errorMessage)
        *errorMessage = QObject::tr("Das Schreiben von Resource-DLLs wird auf dieser Plattform nicht unterstützt.");
    return false;
#endif

    if (outGlobalId)
        *outGlobalId = makeGlobalId(slot, localId);
    return true;
}

} // namespace flatlas::infrastructure
