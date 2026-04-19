#include <QtTest>
#include "infrastructure/io/VmeshDecoder.h"

using namespace flatlas::infrastructure;

class TestVmeshDecoder : public QObject {
    Q_OBJECT
private slots:
    void vertexStrideXYZ()
    {
        QCOMPARE(VmeshDecoder::vertexStride(FVF_XYZ), 12); // 3 floats
    }

    void vertexStrideXYZNormal()
    {
        uint32_t fvf = FVF_XYZ | FVF_NORMAL;
        QCOMPARE(VmeshDecoder::vertexStride(fvf), 24); // 6 floats
    }

    void vertexStrideXYZNormalDiffuseTex1()
    {
        uint32_t fvf = FVF_XYZ | FVF_NORMAL | FVF_DIFFUSE | FVF_TEX1;
        // 3*4 + 3*4 + 4 + 2*4 = 12+12+4+8 = 36
        QCOMPARE(VmeshDecoder::vertexStride(fvf), 36);
    }

    void vertexStrideTex2()
    {
        uint32_t fvf = FVF_XYZ | FVF_TEX2;
        // XYZ=12, TEX2=2*8=16
        QCOMPARE(VmeshDecoder::vertexStride(fvf), 28);
    }

    void uvSetCountNone()
    {
        QCOMPARE(VmeshDecoder::uvSetCount(FVF_XYZ), 0);
    }

    void uvSetCountOne()
    {
        QCOMPARE(VmeshDecoder::uvSetCount(FVF_TEX1), 1);
    }

    void uvSetCountFour()
    {
        QCOMPARE(VmeshDecoder::uvSetCount(FVF_TEX4), 4);
    }

    void decodeEmptyData()
    {
        QByteArray empty;
        auto mesh = VmeshDecoder::decode(empty);
        QVERIFY(mesh.positions.isEmpty());
        QCOMPARE(mesh.numVertices, 0);
    }

    void decodeShortData()
    {
        QByteArray tooShort(10, '\0');
        auto mesh = VmeshDecoder::decode(tooShort);
        QVERIFY(mesh.positions.isEmpty());
    }

    void decodeSyntheticVMeshData()
    {
        // Build a minimal VMeshData block:
        // Header: meshType(4) surfaceType(4) numMeshGroups(2) numRefVertices(2)
        //         fvf(2) numVertices(2)
        // = 16 bytes header
        // Then: meshGroups (each 20 bytes), indices (numRefVertices * 2 bytes), vertices
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds.setFloatingPointPrecision(QDataStream::SinglePrecision);

        // Header
        ds << uint32_t(0x01);     // meshType
        ds << uint32_t(0x04);     // surfaceType
        ds << uint16_t(1);        // numMeshGroups
        ds << uint16_t(3);        // numRefVertices (3 indices)
        uint16_t fvf = FVF_XYZ | FVF_NORMAL;
        ds << fvf;
        ds << uint16_t(3);        // numVertices

        // Mesh group header (16 bytes): materialId(4), startVert(2), endVert(2), numRefVerts(2), padding(6)
        ds << uint32_t(0);        // materialId
        ds << uint16_t(0);        // startVertex
        ds << uint16_t(2);        // endVertex
        ds << uint16_t(3);        // numRefVertices
        ds << uint16_t(0) << uint16_t(0) << uint16_t(0); // padding to 16 bytes total

        // Indices (3 uint16)
        ds << uint16_t(0) << uint16_t(1) << uint16_t(2);

        // Vertices: 3 vertices with XYZ + NORMAL (6 floats each)
        // v0: pos(0,0,0) normal(0,0,1)
        ds << 0.0f << 0.0f << 0.0f << 0.0f << 0.0f << 1.0f;
        // v1: pos(1,0,0) normal(0,0,1)
        ds << 1.0f << 0.0f << 0.0f << 0.0f << 0.0f << 1.0f;
        // v2: pos(0,1,0) normal(0,0,1)
        ds << 0.0f << 1.0f << 0.0f << 0.0f << 0.0f << 1.0f;

        auto mesh = VmeshDecoder::decode(data);
        QCOMPARE(mesh.numVertices, 3);
        QCOMPARE(mesh.positions.size(), 3);
        QCOMPARE(mesh.normals.size(), 3);
        QCOMPARE(mesh.indices.size(), 3);

        QCOMPARE(mesh.positions[0], QVector3D(0, 0, 0));
        QCOMPARE(mesh.positions[1], QVector3D(1, 0, 0));
        QCOMPARE(mesh.positions[2], QVector3D(0, 1, 0));

        QCOMPARE(mesh.normals[0], QVector3D(0, 0, 1));

        QCOMPARE(mesh.indices[0], 0u);
        QCOMPARE(mesh.indices[1], 1u);
        QCOMPARE(mesh.indices[2], 2u);
    }
};

QTEST_GUILESS_MAIN(TestVmeshDecoder)
#include "test_VmeshDecoder.moc"
