#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QDebug>
#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestSimpleShipModel : public QObject {
    Q_OBJECT
private slots:
    void loadCvStarflierCmp()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local CV Starflier model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        qDebug() << "warnings" << decoded.warnings;
        qDebug() << "parts" << decoded.parts.size() << "vmeshRefs" << decoded.vmeshRefs.size()
                 << "vmeshBlocks" << decoded.vmeshDataBlocks.size();
        for (const auto &part : decoded.parts) {
            qDebug() << "part" << part.name
                     << "parent" << part.parentPartName
                     << "source" << part.sourceName
                     << "object" << part.objectName;
        }
        for (const auto &block : decoded.vmeshDataBlocks) {
            qDebug() << "block path" << block.nodePath
                     << "source" << block.sourceName
                     << "family" << block.familyKey
                     << "stride" << block.strideHint
                     << "kind" << block.headerHint.structureKind;
        }
        for (const auto &ref : decoded.vmeshRefs) {
            qDebug() << "ref path" << ref.nodePath
                     << "meshRef" << Qt::hex << ref.meshDataReference << Qt::dec
                     << "parent" << ref.parentName
                     << "source" << ref.matchedSourceName
                     << "model" << ref.modelName
                     << "level" << ref.levelName
                     << "groups" << ref.groupStart << ref.groupCount;
        }
        QVERIFY2(decoded.isValid(), "Decoded ship model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);

        int nodeCount = 0;
        int meshCount = 0;
        int nonRootMeshNodes = 0;
        std::function<void(const ModelNode &, int)> walk = [&](const ModelNode &node, int depth) {
            ++nodeCount;
            qDebug() << "node depth" << depth
                     << "name" << node.name
                     << "meshes" << node.meshes.size()
                     << "children" << node.children.size()
                     << "origin" << node.origin
                     << "rotation scalar" << node.rotation.scalar();
            if (depth > 0 && !node.meshes.isEmpty())
                ++nonRootMeshNodes;
            for (const auto &mesh : node.meshes) {
                ++meshCount;
                qDebug() << "mesh vertices" << mesh.vertices.size()
                         << "indices" << mesh.indices.size()
                         << "material" << mesh.materialName
                         << "texture" << mesh.textureName;
                QVERIFY(mesh.vertices.size() > 0);
                QVERIFY(mesh.indices.size() > 0);
                QVERIFY(mesh.indices.size() % 3 == 0);
                const auto bounds = flatlas::rendering::ModelGeometryBuilder::boundsForMesh(mesh);
                QVERIFY(bounds.valid);
            }
            for (const auto &child : node.children)
                walk(child, depth + 1);
        };

        walk(decoded.rootNode, 0);

        QVERIFY(meshCount > 0);
        QVERIFY(decoded.rootNode.children.size() > 0);
        QVERIFY(nodeCount > 1);
        QVERIFY(nonRootMeshNodes > 0);

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestSimpleShipModel)
#include "test_SimpleShipModel.moc"
