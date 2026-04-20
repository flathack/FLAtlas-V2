// infrastructure/io/PeVersionInfo.cpp – PE executable version information reader

#include "PeVersionInfo.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace flatlas::infrastructure {

PeVersionInfo::PeVersionInfo(const QString &filePath)
{
    load(filePath);
}

void PeVersionInfo::load(const QString &filePath)
{
    m_valid = false;

#ifdef Q_OS_WIN
    const auto widePath = reinterpret_cast<LPCWSTR>(filePath.utf16());

    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(widePath, &dummy);
    if (size == 0)
        return;

    QByteArray buffer(static_cast<int>(size), '\0');
    if (!GetFileVersionInfoW(widePath, 0, size, buffer.data()))
        return;

    // --- Fixed version info (VS_FIXEDFILEINFO) ---
    VS_FIXEDFILEINFO *ffi = nullptr;
    UINT ffiLen = 0;
    if (VerQueryValueW(buffer.data(), L"\\",
                       reinterpret_cast<LPVOID *>(&ffi), &ffiLen)
        && ffi) {
        m_major = HIWORD(ffi->dwFileVersionMS);
        m_minor = LOWORD(ffi->dwFileVersionMS);
        m_patch = HIWORD(ffi->dwFileVersionLS);
        m_build = LOWORD(ffi->dwFileVersionLS);
        m_fileVersion = QStringLiteral("%1.%2.%3.%4")
                            .arg(m_major).arg(m_minor).arg(m_patch).arg(m_build);
        m_valid = true;
    }

    // --- String table queries ---
    auto queryString = [&](const wchar_t *name) -> QString {
        // Try common language/codepage pairs
        static const wchar_t *prefixes[] = {
            L"\\StringFileInfo\\040904b0\\",   // US English, Unicode
            L"\\StringFileInfo\\040904E4\\",   // US English, Western European
            L"\\StringFileInfo\\000004b0\\",   // Language neutral, Unicode
        };

        for (const auto *prefix : prefixes) {
            std::wstring subBlock = prefix;
            subBlock += name;

            wchar_t *val = nullptr;
            UINT valLen = 0;
            if (VerQueryValueW(buffer.data(), subBlock.c_str(),
                               reinterpret_cast<LPVOID *>(&val), &valLen)
                && val && valLen > 0) {
                return QString::fromWCharArray(val, static_cast<int>(valLen)).trimmed();
            }
        }
        return {};
    };

    m_productVersion  = queryString(L"ProductVersion");
    m_productName     = queryString(L"ProductName");
    m_companyName     = queryString(L"CompanyName");
    m_fileDescription = queryString(L"FileDescription");
    m_originalFilename = queryString(L"OriginalFilename");
    m_internalName    = queryString(L"InternalName");

    if (m_productVersion.isEmpty())
        m_productVersion = m_fileVersion;

#else
    Q_UNUSED(filePath);
#endif
}

} // namespace flatlas::infrastructure
