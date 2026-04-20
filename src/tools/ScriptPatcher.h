#pragma once
// tools/ScriptPatcher.h – Freelancer EXE binary patching (OpenSP, Resolution)

#include <QString>
#include <QByteArray>

namespace flatlas::tools {

/// Result of a patch operation.
struct PatchResult {
    bool    success = false;
    QString errorMessage;
    QString backupPath;   ///< Path to backup file created before patching.
};

/// Binary patches for Freelancer.exe.
///
/// All patching methods create a backup of the original file before modifying.
/// Offsets are validated against known EXE signatures before writing.
class ScriptPatcher {
public:
    /// Apply the Open-SP patch (removes single-player restriction).
    /// The standard Freelancer.exe (v1.1) has a JNZ at offset 0x1B0A24
    /// that is changed to JMP to skip the SP server check.
    static PatchResult applyOpenSpPatch(const QString &exePath);

    /// Write a custom resolution into the EXE.
    /// Standard Freelancer.exe stores width at 0x1B0B48 and height at 0x1B0B4C
    /// as 32-bit little-endian integers.
    static PatchResult applyResolutionPatch(const QString &exePath, int width, int height);

    /// Revert a patch by restoring from the backup file.
    static PatchResult revertPatch(const QString &exePath, const QString &backupPath);

    /// Create a backup of the EXE file (copies to exePath + ".bak").
    static QString createBackup(const QString &exePath);

    /// Read bytes at a given offset from a file.
    static QByteArray readBytes(const QString &filePath, qint64 offset, qint64 count);

    /// Write bytes at a given offset into a file.
    static bool writeBytes(const QString &filePath, qint64 offset, const QByteArray &data);
};

} // namespace flatlas::tools
