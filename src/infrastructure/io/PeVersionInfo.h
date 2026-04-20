#pragma once
// infrastructure/io/PeVersionInfo.h – PE executable version information reader

#include <QString>

namespace flatlas::infrastructure {

/// Reads VS_VERSIONINFO from a Windows PE executable or DLL.
class PeVersionInfo {
public:
    explicit PeVersionInfo(const QString &filePath);

    bool isValid() const { return m_valid; }

    /// File version as "major.minor.patch.build"
    QString fileVersion() const { return m_fileVersion; }

    /// Product version string
    QString productVersion() const { return m_productVersion; }

    /// Product name from StringFileInfo
    QString productName() const { return m_productName; }

    /// Company name from StringFileInfo
    QString companyName() const { return m_companyName; }

    /// File description from StringFileInfo
    QString fileDescription() const { return m_fileDescription; }

    /// Original filename from StringFileInfo
    QString originalFilename() const { return m_originalFilename; }

    /// Internal name from StringFileInfo
    QString internalName() const { return m_internalName; }

    /// Raw major/minor/patch/build components
    int majorVersion() const { return m_major; }
    int minorVersion() const { return m_minor; }
    int patchVersion() const { return m_patch; }
    int buildNumber()  const { return m_build; }

private:
    void load(const QString &filePath);

    bool    m_valid = false;
    QString m_fileVersion;
    QString m_productVersion;
    QString m_productName;
    QString m_companyName;
    QString m_fileDescription;
    QString m_originalFilename;
    QString m_internalName;
    int     m_major = 0;
    int     m_minor = 0;
    int     m_patch = 0;
    int     m_build = 0;
};

} // namespace flatlas::infrastructure
