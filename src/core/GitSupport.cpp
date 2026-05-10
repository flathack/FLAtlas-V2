#include "GitSupport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QTextStream>

namespace flatlas::core {
namespace {

struct GitResult
{
    int exitCode = -1;
    QString output;
    QString error;
};

GitResult runGit(const QStringList &arguments, const QString &workingDirectory = {})
{
    QProcess process;
    if (!workingDirectory.isEmpty())
        process.setWorkingDirectory(workingDirectory);
    process.start(QStringLiteral("git"), arguments);
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return {-1, QString::fromLocal8Bit(process.readAllStandardOutput()), QStringLiteral("Git command timed out.")};
    }
    return {
        process.exitCode(),
        QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed(),
        QString::fromLocal8Bit(process.readAllStandardError()).trimmed()
    };
}

QString resolveWorkTreeRoot(const QString &path)
{
    if (path.trimmed().isEmpty() || !QFileInfo::exists(path))
        return {};
    const GitResult result = runGit({QStringLiteral("-C"), QDir::toNativeSeparators(path),
                                     QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")});
    if (result.exitCode != 0)
        return {};
    return QDir::cleanPath(result.output);
}

QString normalizeGitPath(QString path)
{
    path = path.trimmed();
    if (path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"')))
        path = path.mid(1, path.size() - 2);
    return path;
}

int countTextLines(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    int lines = 0;
    while (!file.atEnd()) {
        file.readLine();
        ++lines;
        if (lines > 100000)
            break;
    }
    return lines;
}

} // namespace

int GitStatus::addedLineCount() const
{
    int total = 0;
    for (const GitFileChange &file : files)
        total += file.addedLines;
    return total;
}

int GitStatus::deletedLineCount() const
{
    int total = 0;
    for (const GitFileChange &file : files)
        total += file.deletedLines;
    return total;
}

bool GitSupport::isGitAvailable(QString *versionText)
{
    const GitResult result = runGit({QStringLiteral("--version")});
    if (versionText)
        *versionText = result.output;
    return result.exitCode == 0;
}

GitStatus GitSupport::statusForPath(const QString &path)
{
    GitStatus status;
    status.gitAvailable = isGitAvailable();
    if (!status.gitAvailable)
        return status;

    status.workTreeRoot = resolveWorkTreeRoot(path);
    status.repository = !status.workTreeRoot.isEmpty();
    if (!status.repository)
        return status;

    const GitResult diff = runGit({QStringLiteral("-C"), status.workTreeRoot,
                                   QStringLiteral("diff"), QStringLiteral("--numstat"), QStringLiteral("HEAD"), QStringLiteral("--")});
    if (diff.exitCode != 0) {
        status.errorMessage = diff.error;
    }

    QSet<QString> knownPaths;
    if (diff.exitCode == 0) {
        const QStringList lines = diff.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList parts = line.split(QLatin1Char('\t'));
            if (parts.size() < 3)
                continue;

            GitFileChange change;
            change.addedLines = parts.at(0) == QStringLiteral("-") ? 0 : parts.at(0).toInt();
            change.deletedLines = parts.at(1) == QStringLiteral("-") ? 0 : parts.at(1).toInt();
            change.path = normalizeGitPath(parts.mid(2).join(QLatin1Char('\t')));
            if (change.path.isEmpty())
                continue;
            knownPaths.insert(change.path);
            status.files.append(change);
        }
    }

    const GitResult untracked = runGit({QStringLiteral("-C"), status.workTreeRoot,
                                        QStringLiteral("ls-files"), QStringLiteral("--others"), QStringLiteral("--exclude-standard")});
    if (untracked.exitCode == 0) {
        const QStringList untrackedFiles = untracked.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &pathLine : untrackedFiles) {
            const QString relativePath = normalizeGitPath(pathLine);
            if (relativePath.isEmpty() || knownPaths.contains(relativePath))
                continue;

            GitFileChange change;
            change.path = relativePath;
            change.untracked = true;
            change.addedLines = countTextLines(QDir(status.workTreeRoot).absoluteFilePath(relativePath));
            status.files.append(change);
        }
    }

    return status;
}

QVector<GitCommitInfo> GitSupport::recentCommitsForPath(const QString &path, int maxCount)
{
    QVector<GitCommitInfo> commits;
    if (!isGitAvailable())
        return commits;

    const QString root = resolveWorkTreeRoot(path);
    if (root.isEmpty())
        return commits;

    const GitResult result = runGit({
        QStringLiteral("-C"), root,
        QStringLiteral("log"),
        QStringLiteral("--max-count=%1").arg(qMax(1, maxCount)),
        QStringLiteral("--pretty=format:%h%x1f%an%x1f%ar%x1f%s")
    });
    if (result.exitCode != 0)
        return commits;

    const QStringList lines = result.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QChar(0x1f));
        if (parts.size() < 4)
            continue;
        GitCommitInfo commit;
        commit.shortHash = parts.at(0);
        commit.author = parts.at(1);
        commit.relativeDate = parts.at(2);
        commit.subject = parts.mid(3).join(QStringLiteral(" "));
        commits.append(commit);
    }
    return commits;
}

bool GitSupport::ensureFlatlasGitIgnore(const QString &repoRoot, QString *errorMessage)
{
    QDir dir(repoRoot);
    if (!dir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Repository folder does not exist.");
        return false;
    }

    const QString gitignorePath = dir.absoluteFilePath(QStringLiteral(".gitignore"));
    QFile file(gitignorePath);
    QString content;
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage)
                *errorMessage = QStringLiteral(".gitignore could not be read.");
            return false;
        }
        content = QString::fromUtf8(file.readAll());
        file.close();
    }

    const QStringList required = {
        QStringLiteral("# flatlas"),
        QStringLiteral(".flatlas"),
        QStringLiteral(".FLatlasLauncher"),
        QStringLiteral(".flatlas_mod_settings.json")
    };

    QStringList missing;
    for (const QString &line : required) {
        if (!content.split(QLatin1Char('\n')).contains(line))
            missing << line;
    }
    if (missing.isEmpty())
        return true;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral(".gitignore could not be written.");
        return false;
    }
    QTextStream stream(&file);
    if (!content.isEmpty() && !content.endsWith(QLatin1Char('\n')))
        stream << '\n';
    for (const QString &line : missing)
        stream << line << '\n';
    return true;
}

bool GitSupport::initializeRepository(const QString &path, QString *errorMessage)
{
    if (!isGitAvailable()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Git is not installed.");
        return false;
    }
    QDir dir(path);
    if (!dir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Folder does not exist.");
        return false;
    }

    GitResult result = runGit({QStringLiteral("-C"), dir.absolutePath(), QStringLiteral("init")});
    if (result.exitCode != 0) {
        if (errorMessage)
            *errorMessage = result.error;
        return false;
    }

    if (!ensureFlatlasGitIgnore(dir.absolutePath(), errorMessage))
        return false;

    result = runGit({QStringLiteral("-C"), dir.absolutePath(), QStringLiteral("add"), QStringLiteral(".")});
    if (result.exitCode != 0) {
        if (errorMessage)
            *errorMessage = result.error;
        return false;
    }

    result = runGit({QStringLiteral("-C"), dir.absolutePath(),
                     QStringLiteral("-c"), QStringLiteral("user.name=FLAtlas"),
                     QStringLiteral("-c"), QStringLiteral("user.email=flatlas@local"),
                     QStringLiteral("commit"),
                     QStringLiteral("-m"), QStringLiteral("Initial FLAtlas commit")});
    if (result.exitCode != 0) {
        if (errorMessage)
            *errorMessage = result.error.isEmpty() ? result.output : result.error;
        return false;
    }
    return true;
}

} // namespace flatlas::core
