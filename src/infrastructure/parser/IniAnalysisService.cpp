#include "IniAnalysisService.h"

#include "BiniDecoder.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>

namespace flatlas::infrastructure {

namespace {

QString normalizedName(QString value)
{
    return value.trimmed().toLower();
}

QString decodeText(const QByteArray &raw)
{
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString utf8Text = utf8(raw);
    if (!utf8.hasError())
        return utf8Text;
    return QString::fromLatin1(raw);
}

bool isCommentLine(const QString &trimmed)
{
    return trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QStringLiteral("//"));
}

void appendDiagnostic(QVector<IniDiagnostic> *diagnostics,
                      IniDiagnosticSeverity severity,
                      const QString &code,
                      const QString &message,
                      int lineNumber,
                      const QString &sectionName = QString(),
                      const QString &keyName = QString())
{
    IniDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.lineNumber = lineNumber;
    diagnostic.sectionName = sectionName;
    diagnostic.keyName = keyName;
    diagnostics->append(diagnostic);
}

bool matchesFilter(const QString &value, const QString &filter)
{
    return filter.trimmed().isEmpty() || value.contains(filter, Qt::CaseInsensitive);
}

bool lineMatchesOptions(const QString &line,
                        const QString &sectionName,
                        const QString &keyName,
                        const IniSearchOptions &options)
{
    if (!matchesFilter(sectionName, options.sectionFilter))
        return false;
    if (!matchesFilter(keyName, options.keyFilter))
        return false;

    const QString query = options.query;
    if (query.trimmed().isEmpty())
        return false;

    if (options.useRegex) {
        QRegularExpression::PatternOptions regexOptions = QRegularExpression::NoPatternOption;
        if (!options.caseSensitive)
            regexOptions |= QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression regex(query, regexOptions);
        return regex.isValid() && regex.match(line).hasMatch();
    }

    return line.contains(query, options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

} // namespace

int IniAnalysisResult::sectionIndexForLine(int lineNumber) const
{
    for (int index = 0; index < sections.size(); ++index) {
        const auto &section = sections.at(index);
        if (lineNumber >= section.startLine && lineNumber <= section.endLine)
            return index;
    }
    return -1;
}

QString IniAnalysisService::loadIniLikeText(const QString &filePath, bool *wasBini)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (wasBini)
            *wasBini = false;
        return {};
    }

    const QByteArray raw = file.readAll();
    file.close();

    const bool bini = BiniDecoder::isBini(raw);
    if (wasBini)
        *wasBini = bini;
    if (bini)
        return BiniDecoder::decode(raw);
    return decodeText(raw);
}

IniAnalysisResult IniAnalysisService::analyzeText(const QString &text)
{
    IniAnalysisResult result;

    const QStringList lines = text.split(QLatin1Char('\n'));
    int currentSectionIndex = -1;
    QHash<QString, int> sectionCounts;
    QHash<QString, QHash<QString, int>> keyCountsBySection;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        line.remove(QChar('\r'));
        const QString trimmed = line.trimmed();
        const int lineNumber = i + 1;

        if (trimmed.isEmpty() || isCommentLine(trimmed))
            continue;

        if (trimmed.startsWith(QLatin1Char('['))) {
            if (!trimmed.endsWith(QLatin1Char(']'))) {
                appendDiagnostic(&result.diagnostics,
                                 IniDiagnosticSeverity::Error,
                                 QStringLiteral("malformed-section"),
                                 QStringLiteral("Section header is missing a closing ']'."),
                                 lineNumber);
                continue;
            }

            const QString sectionName = trimmed.mid(1, trimmed.size() - 2).trimmed();
            if (sectionName.isEmpty()) {
                appendDiagnostic(&result.diagnostics,
                                 IniDiagnosticSeverity::Error,
                                 QStringLiteral("empty-section-name"),
                                 QStringLiteral("Section name must not be empty."),
                                 lineNumber);
            }

            if (currentSectionIndex >= 0)
                result.sections[currentSectionIndex].endLine = lineNumber - 1;

            IniSectionInfo section;
            section.name = sectionName;
            section.startLine = lineNumber;
            section.endLine = lineNumber;
            const QString normalizedSection = normalizedName(sectionName);
            sectionCounts[normalizedSection] += 1;
            section.duplicateSection = sectionCounts.value(normalizedSection) > 1;
            if (section.duplicateSection) {
                appendDiagnostic(&result.diagnostics,
                                 IniDiagnosticSeverity::Warning,
                                 QStringLiteral("duplicate-section"),
                                 QStringLiteral("Section '%1' exists multiple times.").arg(sectionName),
                                 lineNumber,
                                 sectionName);
            }

            result.sections.append(section);
            currentSectionIndex = result.sections.size() - 1;
            continue;
        }

        if (currentSectionIndex < 0) {
            appendDiagnostic(&result.diagnostics,
                             IniDiagnosticSeverity::Error,
                             QStringLiteral("entry-outside-section"),
                             QStringLiteral("Key/value entry is outside of any section."),
                             lineNumber);
            continue;
        }

        const int eqPos = line.indexOf(QLatin1Char('='));
        if (eqPos < 0) {
            appendDiagnostic(&result.diagnostics,
                             IniDiagnosticSeverity::Error,
                             QStringLiteral("missing-equals"),
                             QStringLiteral("Key/value line is missing '='."),
                             lineNumber,
                             result.sections.at(currentSectionIndex).name);
            continue;
        }

        QString key = line.left(eqPos).trimmed();
        QString value = line.mid(eqPos + 1).trimmed();
        const int commentIndex = value.indexOf(QLatin1Char(';'));
        if (commentIndex >= 0)
            value = value.left(commentIndex).trimmed();

        if (key.isEmpty()) {
            appendDiagnostic(&result.diagnostics,
                             IniDiagnosticSeverity::Error,
                             QStringLiteral("empty-key-name"),
                             QStringLiteral("Key name must not be empty."),
                             lineNumber,
                             result.sections.at(currentSectionIndex).name);
            continue;
        }

        auto &section = result.sections[currentSectionIndex];
        section.endLine = lineNumber;
        section.keys.append({key, value, lineNumber});

        const QString normalizedSection = normalizedName(section.name);
        const QString normalizedKey = normalizedName(key);
        keyCountsBySection[normalizedSection][normalizedKey] += 1;
        if (keyCountsBySection[normalizedSection].value(normalizedKey) > 1) {
            appendDiagnostic(&result.diagnostics,
                             IniDiagnosticSeverity::Warning,
                             QStringLiteral("duplicate-key"),
                             QStringLiteral("Key '%1' exists multiple times in section '%2'.").arg(key, section.name),
                             lineNumber,
                             section.name,
                             key);
        }
    }

    if (!result.sections.isEmpty())
        result.sections.last().endLine = qMax(result.sections.last().endLine, lines.size());

    return result;
}

QString IniAnalysisService::sectionText(const QString &text, const IniSectionInfo &section)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    if (section.startLine <= 0 || section.endLine < section.startLine || section.startLine > lines.size())
        return {};

    QStringList result;
    for (int line = section.startLine; line <= qMin(section.endLine, lines.size()); ++line)
        result.append(lines.at(line - 1));
    return result.join(QLatin1Char('\n'));
}

QVector<IniSearchMatch> IniAnalysisService::searchDirectory(const QString &rootPath, const IniSearchOptions &options)
{
    QVector<IniSearchMatch> matches;
    if (rootPath.trimmed().isEmpty() || options.query.trimmed().isEmpty())
        return matches;

    QDir root(rootPath);
    if (!root.exists())
        return matches;

    QDirIterator it(rootPath,
                    options.nameFilters,
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        bool wasBini = false;
        const QString text = loadIniLikeText(filePath, &wasBini);
        if (text.isEmpty() && QFileInfo(filePath).size() > 0)
            continue;

        QString currentSection;
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            QString line = lines.at(i);
            line.remove(QChar('\r'));
            const QString trimmed = line.trimmed();
            if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
                currentSection = trimmed.mid(1, trimmed.size() - 2).trimmed();
            }

            QString keyName;
            const int eqPos = line.indexOf(QLatin1Char('='));
            if (eqPos > 0)
                keyName = line.left(eqPos).trimmed();

            if (!lineMatchesOptions(line, currentSection, keyName, options))
                continue;

            IniSearchMatch match;
            match.filePath = filePath;
            match.relativePath = root.relativeFilePath(filePath);
            match.sectionName = currentSection;
            match.keyName = keyName;
            match.lineNumber = i + 1;
            match.lineText = trimmed;
            matches.append(match);
        }
    }

    return matches;
}

} // namespace flatlas::infrastructure