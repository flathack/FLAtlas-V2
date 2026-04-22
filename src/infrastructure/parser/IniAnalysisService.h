#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::infrastructure {

enum class IniDiagnosticSeverity {
    Info,
    Warning,
    Error,
};

struct IniKeyInfo {
    QString key;
    QString value;
    int lineNumber = 0;
};

struct IniSectionInfo {
    QString name;
    int startLine = 0;
    int endLine = 0;
    bool duplicateSection = false;
    QVector<IniKeyInfo> keys;
};

struct IniDiagnostic {
    IniDiagnosticSeverity severity = IniDiagnosticSeverity::Info;
    QString code;
    QString message;
    int lineNumber = 0;
    QString sectionName;
    QString keyName;
};

struct IniSearchOptions {
    QString query;
    bool useRegex = false;
    bool caseSensitive = false;
    QString sectionFilter;
    QString keyFilter;
    QStringList nameFilters = {QStringLiteral("*.ini"), QStringLiteral("*.cfg"), QStringLiteral("*.txt"), QStringLiteral("*.bini")};
};

struct IniSearchMatch {
    QString filePath;
    QString relativePath;
    QString sectionName;
    QString keyName;
    int lineNumber = 0;
    QString lineText;
};

struct IniAnalysisResult {
    QVector<IniSectionInfo> sections;
    QVector<IniDiagnostic> diagnostics;

    int sectionIndexForLine(int lineNumber) const;
};

class IniAnalysisService
{
public:
    static QString loadIniLikeText(const QString &filePath, bool *wasBini = nullptr);
    static IniAnalysisResult analyzeText(const QString &text);
    static QString sectionText(const QString &text, const IniSectionInfo &section);
    static QVector<IniSearchMatch> searchDirectory(const QString &rootPath, const IniSearchOptions &options);
};

} // namespace flatlas::infrastructure