#pragma once

#include <QString>
#include <QVector>

namespace flatlas::core {

struct GitFileChange
{
    QString path;
    int addedLines = 0;
    int deletedLines = 0;
    bool untracked = false;
};

struct GitStatus
{
    bool gitAvailable = false;
    bool repository = false;
    QString workTreeRoot;
    QString errorMessage;
    QVector<GitFileChange> files;

    int changedFileCount() const { return files.size(); }
    int addedLineCount() const;
    int deletedLineCount() const;
    bool hasChanges() const { return !files.isEmpty(); }
};

struct GitCommitInfo
{
    QString shortHash;
    QString author;
    QString relativeDate;
    QString subject;
};

class GitSupport
{
public:
    static bool isGitAvailable(QString *versionText = nullptr);
    static GitStatus statusForPath(const QString &path);
    static QString diffForFile(const QString &path, const QString &relativeFilePath, QString *errorMessage = nullptr);
    static QVector<GitCommitInfo> recentCommitsForPath(const QString &path, int maxCount = 8);
    static bool ensureFlatlasGitIgnore(const QString &repoRoot, QString *errorMessage = nullptr);
    static bool initializeRepository(const QString &path, QString *errorMessage = nullptr);
};

} // namespace flatlas::core
