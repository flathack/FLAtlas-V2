#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QString>
#include <QStringList>

#include "infrastructure/io/CmpLoader.h"

namespace flatlas::infrastructure {

class FreelancerMaterialResolver {
public:
    static QString resolveTexturePathForMesh(const QString &modelPath, const MeshData &mesh);
    static QImage loadTextureForMesh(const QString &modelPath, const MeshData &mesh);

private:
    static QHash<QString, QStringList> extractUtfMaterialTextureMap(const QString &utfPath);
    static QHash<QString, QImage> extractUtfEmbeddedTextures(const QString &utfPath);
    static QString resolveTextureValue(const QString &sourcePath, const QString &value);
    static QString findDataRoot(const QString &path);
    static QStringList textureCandidatesForMesh(const QString &modelPath, const MeshData &mesh);
    static QImage resolveEmbeddedTextureForMesh(const QString &modelPath, const MeshData &mesh);

    static QMutex s_cacheMutex;
    static QHash<QString, QHash<QString, QStringList>> s_materialTextureMapCache;
    static QHash<QString, QHash<QString, QImage>> s_embeddedTextureCache;
    // Cache expensive recursive dataRoot scans per (dataRoot, baseName).
    // Without this, opening li_manhattan_bar.cmp re-scans the whole DATA tree
    // for every unresolved texture candidate, blocking the UI for tens of seconds.
    static QHash<QString, QHash<QString, QString>> s_dataRootFileScanCache;
    static QHash<QString, bool> s_dataRootScannedFlag;
};

} // namespace flatlas::infrastructure
