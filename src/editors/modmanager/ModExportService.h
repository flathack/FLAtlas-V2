#pragma once

#include <QVector>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

namespace flatlas::editors {

struct ModExportFile {
    QString sourcePath;
    QString relativePath;
    QString status; // "new" or "modified"
    qint64 size = 0;
    QString sha256;
    QString referenceSha256;
};

struct ModExportPlan {
    QString modRoot;
    QString referenceRoot;
    QVector<ModExportFile> files;
    int unchangedCount = 0;
    QStringList errors;

    QVector<ModExportFile> exportFiles() const;
    int newCount() const;
    int modifiedCount() const;
};

class ModExportService {
public:
    using ProgressCallback = std::function<bool(const QString &stage, int current, int total, const QString &path)>;

    static QString normalizeArchivePath(const QString &path);
    static QStringList automaticExclusionLabels();
    static bool isAutomaticallyExcluded(const QString &relativePath);
    static ModExportPlan collectChangedFiles(const QString &modRoot,
                                             const QString &referenceRoot,
                                             const ProgressCallback &progress = {});
    static ModExportPlan filterPlan(const ModExportPlan &plan, const QSet<QString> &excludedRelativePaths);
    static QString defaultScriptXml(const QString &name,
                                    const QString &author,
                                    const QString &description,
                                    bool saveSafe);
    static bool writeZip(const ModExportPlan &plan,
                         const QString &targetPath,
                         QString *errorMessage = nullptr,
                         const ProgressCallback &progress = {});
    static bool writeFlmod(const ModExportPlan &plan,
                           const QString &targetPath,
                           const QString &scriptXml,
                           QString *errorMessage = nullptr,
                           const ProgressCallback &progress = {});
};

} // namespace flatlas::editors
