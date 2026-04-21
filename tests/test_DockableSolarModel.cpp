#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QDebug>
#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestDockableSolarModel : public QObject {
    Q_OBJECT
private slots:
    void loadDockingRingX2Cmp()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local docking_ringx2_lod model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        qDebug() << "warnings" << decoded.warnings;
        qDebug() << "parts" << decoded.parts.size()
                 << "vmeshRefs" << decoded.vmeshRefs.size()
                 << "vmeshBlocks" << decoded.vmeshDataBlocks.size();
        int familyPlanRefs = 0;
        for (const auto &ref : decoded.vmeshRefs) {
            qDebug() << "ref path" << ref.nodePath
                     << "source" << ref.matchedSourceName
                     << "resolution" << ref.resolutionHint
                     << "usedFamily" << ref.usedStructuredFamilyFallback
                     << "debug" << ref.debugHint;
            if (ref.usedStructuredFamilyFallback && !ref.debugHint.isEmpty())
                ++familyPlanRefs;
        }
        QVERIFY2(decoded.isValid(), "Decoded dockable solar model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);

        int meshCount = 0;
        int childMeshNodes = 0;
        int familyPlanMeshes = 0;
        std::function<void(const ModelNode &, int)> walk = [&](const ModelNode &node, int depth) {
            qDebug() << "node depth" << depth
                     << "name" << node.name
                     << "meshes" << node.meshes.size()
                     << "children" << node.children.size();
            if (depth > 0 && !node.meshes.isEmpty())
                ++childMeshNodes;
            for (const auto &mesh : node.meshes) {
                ++meshCount;
                qDebug() << "mesh vertices" << mesh.vertices.size()
                         << "indices" << mesh.indices.size()
                         << "material" << mesh.materialName
                         << "debug" << mesh.debugHint;
                QVERIFY(mesh.vertices.size() > 0);
                QVERIFY(mesh.indices.size() > 0);
                QVERIFY(mesh.indices.size() % 3 == 0);
                if (!mesh.debugHint.isEmpty())
                    ++familyPlanMeshes;
                const auto bounds = flatlas::rendering::ModelGeometryBuilder::boundsForMesh(mesh);
                QVERIFY(bounds.valid);
            }
            for (const auto &child : node.children)
                walk(child, depth + 1);
        };

        walk(decoded.rootNode, 0);

        QVERIFY(meshCount > 0);
        QVERIFY(decoded.rootNode.children.size() > 0);
        QVERIFY(childMeshNodes > 0);
        QVERIFY(familyPlanMeshes > 0);
        QVERIFY(familyPlanRefs > 0);

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestDockableSolarModel)
#include "test_DockableSolarModel.moc"
