#pragma once
// infrastructure/io/CmpLoader.h – CMP/3DB-Modell-Loader
// TODO Phase 8
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVector3D>

namespace flatlas::infrastructure {

struct MeshVertex { QVector3D position; QVector3D normal; float u = 0; float v = 0; };
struct MeshData { QVector<MeshVertex> vertices; QVector<uint32_t> indices; QString materialName; };
struct ModelNode { QString name; QVector<MeshData> meshes; QVector<ModelNode> children; };

class CmpLoader {
public:
    static ModelNode loadCmp(const QString &filePath);
    static ModelNode load3db(const QString &filePath);
};

} // namespace flatlas::infrastructure
