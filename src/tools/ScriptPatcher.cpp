// tools/ScriptPatcher.cpp – Freelancer EXE binary patching

#include "ScriptPatcher.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDataStream>

namespace flatlas::tools {

// ---------------------------------------------------------------------------
// Known offsets for standard Freelancer.exe v1.1 (3,�56,832 bytes)
// ---------------------------------------------------------------------------
static constexpr qint64 kOpenSpOffset       = 0x1B0A24;
static constexpr char   kOpenSpOrigByte     = '\x75';   // JNZ
static constexpr char   kOpenSpPatchByte    = '\xEB';   // JMP (unconditional)

static constexpr qint64 kResWidthOffset     = 0x1B0B48;
static constexpr qint64 kResHeightOffset    = 0x1B0B4C;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString ScriptPatcher::createBackup(const QString &exePath)
{
    const QString bakPath = exePath + QStringLiteral(".bak");
    if (QFile::exists(bakPath))
        QFile::remove(bakPath);
    if (!QFile::copy(exePath, bakPath))
        return {};
    return bakPath;
}

QByteArray ScriptPatcher::readBytes(const QString &filePath, qint64 offset, qint64 count)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    if (!f.seek(offset))
        return {};
    return f.read(count);
}

bool ScriptPatcher::writeBytes(const QString &filePath, qint64 offset, const QByteArray &data)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadWrite))
        return false;
    if (!f.seek(offset))
        return false;
    return f.write(data) == data.size();
}

// ---------------------------------------------------------------------------
// OpenSP Patch
// ---------------------------------------------------------------------------

PatchResult ScriptPatcher::applyOpenSpPatch(const QString &exePath)
{
    PatchResult r;

    if (!QFile::exists(exePath)) {
        r.errorMessage = QStringLiteral("File not found: %1").arg(exePath);
        return r;
    }

    // Verify the byte at the offset is the expected original
    QByteArray orig = readBytes(exePath, kOpenSpOffset, 1);
    if (orig.isEmpty()) {
        r.errorMessage = QStringLiteral("Cannot read EXE at offset 0x%1")
                             .arg(kOpenSpOffset, 0, 16);
        return r;
    }

    if (orig.at(0) == kOpenSpPatchByte) {
        r.errorMessage = QStringLiteral("OpenSP patch already applied.");
        return r;
    }

    if (orig.at(0) != kOpenSpOrigByte) {
        r.errorMessage = QStringLiteral("Unexpected byte at offset 0x%1: 0x%2 (expected 0x%3). "
                                        "EXE version may not be supported.")
                             .arg(kOpenSpOffset, 0, 16)
                             .arg(static_cast<quint8>(orig.at(0)), 2, 16, QChar('0'))
                             .arg(static_cast<quint8>(kOpenSpOrigByte), 2, 16, QChar('0'));
        return r;
    }

    // Create backup
    r.backupPath = createBackup(exePath);
    if (r.backupPath.isEmpty()) {
        r.errorMessage = QStringLiteral("Failed to create backup.");
        return r;
    }

    // Apply patch
    if (!writeBytes(exePath, kOpenSpOffset, QByteArray(1, kOpenSpPatchByte))) {
        r.errorMessage = QStringLiteral("Failed to write patch byte.");
        return r;
    }

    r.success = true;
    return r;
}

// ---------------------------------------------------------------------------
// Resolution Patch
// ---------------------------------------------------------------------------

PatchResult ScriptPatcher::applyResolutionPatch(const QString &exePath, int width, int height)
{
    PatchResult r;

    if (!QFile::exists(exePath)) {
        r.errorMessage = QStringLiteral("File not found: %1").arg(exePath);
        return r;
    }

    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        r.errorMessage = QStringLiteral("Invalid resolution: %1x%2").arg(width).arg(height);
        return r;
    }

    // Create backup
    r.backupPath = createBackup(exePath);
    if (r.backupPath.isEmpty()) {
        r.errorMessage = QStringLiteral("Failed to create backup.");
        return r;
    }

    // Write width (little-endian 32-bit)
    QByteArray wData(4, '\0');
    QDataStream wStream(&wData, QIODevice::WriteOnly);
    wStream.setByteOrder(QDataStream::LittleEndian);
    wStream << static_cast<qint32>(width);

    if (!writeBytes(exePath, kResWidthOffset, wData)) {
        r.errorMessage = QStringLiteral("Failed to write width.");
        return r;
    }

    // Write height (little-endian 32-bit)
    QByteArray hData(4, '\0');
    QDataStream hStream(&hData, QIODevice::WriteOnly);
    hStream.setByteOrder(QDataStream::LittleEndian);
    hStream << static_cast<qint32>(height);

    if (!writeBytes(exePath, kResHeightOffset, hData)) {
        r.errorMessage = QStringLiteral("Failed to write height.");
        return r;
    }

    r.success = true;
    return r;
}

// ---------------------------------------------------------------------------
// Revert
// ---------------------------------------------------------------------------

PatchResult ScriptPatcher::revertPatch(const QString &exePath, const QString &backupPath)
{
    PatchResult r;

    if (!QFile::exists(backupPath)) {
        r.errorMessage = QStringLiteral("Backup file not found: %1").arg(backupPath);
        return r;
    }

    if (QFile::exists(exePath) && !QFile::remove(exePath)) {
        r.errorMessage = QStringLiteral("Cannot remove current EXE.");
        return r;
    }

    if (!QFile::copy(backupPath, exePath)) {
        r.errorMessage = QStringLiteral("Failed to restore from backup.");
        return r;
    }

    r.success = true;
    r.backupPath = backupPath;
    return r;
}

} // namespace flatlas::tools
