#pragma once
// infrastructure/io/VmeshDecoder.h – VMESH-Geometrie-Dekodierung
// TODO Phase 8
#include <QByteArray>
#include <QVector>
#include <QVector3D>
namespace flatlas::infrastructure {
class VmeshDecoder {
public:
    struct DecodedMesh { QVector<QVector3D> positions; QVector<QVector3D> normals; QVector<float> uvs; QVector<uint32_t> indices; };
    static DecodedMesh decode(const QByteArray &data, int offset = 0);
};
} // namespace flatlas::infrastructure
