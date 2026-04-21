#include <QtTest/QtTest>

#include "tools/ModelScreenshotExporter.h"

using namespace flatlas::infrastructure;
using namespace flatlas::tools;

namespace {

MeshData makeMesh(const QVector<QVector3D> &positions)
{
    MeshData mesh;
    for (const auto &position : positions)
        mesh.vertices.append(MeshVertex{position, QVector3D(0.0f, 0.0f, 1.0f), 0.0f, 0.0f});

    for (uint32_t index = 0; index < static_cast<uint32_t>(positions.size()); ++index)
        mesh.indices.append(index);

    return mesh;
}

} // namespace

class TestModelScreenshotExporter : public QObject {
    Q_OBJECT

private slots:
    void buildTrianglesUsesFirstMeshFromBestLodPerNode();
    void buildTrianglesAppliesHierarchyTransform();
    void buildTrianglesPrefersCombinedCmpTransformHints();
};

void TestModelScreenshotExporter::buildTrianglesUsesFirstMeshFromBestLodPerNode()
{
    DecodedModel model;
    model.rootNode.name = QStringLiteral("Root");

    ModelNode ship;
    ship.name = QStringLiteral("Ship");
    MeshData lod2 = makeMesh({
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
    });
    lod2.lodIndex = 2;
    ship.meshes.append(lod2);

    MeshData lod0a = makeMesh({
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(2.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 2.0f, 0.0f),
    });
    lod0a.lodIndex = 0;
    ship.meshes.append(lod0a);

    MeshData lod0b = makeMesh({
        QVector3D(2.0f, 2.0f, 0.0f),
        QVector3D(1.0f, 3.0f, 0.0f),
        QVector3D(3.0f, 1.0f, 0.0f),
    });
    lod0b.lodIndex = 0;
    ship.meshes.append(lod0b);
    model.rootNode.children.append(ship);

    const auto triangles = ModelScreenshotExporter::buildTriangles(model);
    QCOMPARE(triangles.size(), 1);
    QCOMPARE(triangles.first().b, QVector3D(2.0f, 0.0f, 0.0f));
    QCOMPARE(triangles.first().c, QVector3D(0.0f, 2.0f, 0.0f));
}

void TestModelScreenshotExporter::buildTrianglesAppliesHierarchyTransform()
{
    DecodedModel model;
    model.rootNode.name = QStringLiteral("Root");
    model.rootNode.origin = QVector3D(5.0f, 0.0f, 0.0f);

    ModelNode child;
    child.name = QStringLiteral("Child");
    child.origin = QVector3D(0.0f, 3.0f, 0.0f);
    child.rotation = QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 0.0f, 1.0f), 90.0f);
    child.meshes.append(makeMesh({
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(2.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 1.0f, 0.0f),
    }));
    model.rootNode.children.append(child);

    const auto triangles = ModelScreenshotExporter::buildTriangles(model);
    QCOMPARE(triangles.size(), 1);
    const auto fuzzyCompare = [](const QVector3D &actual, const QVector3D &expected) {
        return qAbs(actual.x() - expected.x()) < 0.0001f
            && qAbs(actual.y() - expected.y()) < 0.0001f
            && qAbs(actual.z() - expected.z()) < 0.0001f;
    };
    QVERIFY(fuzzyCompare(triangles.first().a, QVector3D(5.0f, 4.0f, 0.0f)));
    QVERIFY(fuzzyCompare(triangles.first().b, QVector3D(5.0f, 5.0f, 0.0f)));
    QVERIFY(fuzzyCompare(triangles.first().c, QVector3D(4.0f, 4.0f, 0.0f)));
}

void TestModelScreenshotExporter::buildTrianglesPrefersCombinedCmpTransformHints()
{
    DecodedModel model;
    model.rootNode.name = QStringLiteral("Root");

    ModelNode child;
    child.name = QStringLiteral("Part_Test");
    child.origin = QVector3D(1.0f, 0.0f, 0.0f);
    child.meshes.append(makeMesh({
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
    }));
    model.rootNode.children.append(child);

    CmpTransformHint hint;
    hint.partName = QStringLiteral("Part_Test");
    hint.hasCombinedTranslation = true;
    hint.combinedTranslation = QVector3D(10.0f, 0.0f, 0.0f);
    model.cmpTransformHints.append(hint);

    const auto triangles = ModelScreenshotExporter::buildTriangles(model);
    QCOMPARE(triangles.size(), 1);
    const auto fuzzyCompare = [](const QVector3D &actual, const QVector3D &expected) {
        return qAbs(actual.x() - expected.x()) < 0.0001f
            && qAbs(actual.y() - expected.y()) < 0.0001f
            && qAbs(actual.z() - expected.z()) < 0.0001f;
    };
    QVERIFY(fuzzyCompare(triangles.first().a, QVector3D(10.0f, 0.0f, 0.0f)));
    QVERIFY(fuzzyCompare(triangles.first().b, QVector3D(11.0f, 0.0f, 0.0f)));
    QVERIFY(fuzzyCompare(triangles.first().c, QVector3D(10.0f, 1.0f, 0.0f)));
}

QTEST_MAIN(TestModelScreenshotExporter)
#include "test_ModelScreenshotExporter.moc"