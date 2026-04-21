#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QDebug>
#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestSimpleShipModel : public QObject {
    Q_OBJECT
private slots:
    void cvStarflierMaterialSignatureSnapshot();
    void loadCoElite2Cmp();
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

void TestSimpleShipModel::loadCoElite2Cmp()
{
    const QString filePath = QStringLiteral(
        "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SHIPS/PIRATE/PI_VHEAVY_FIGHTER/pi_vheavy_fighter.cmp");

    if (!QFileInfo::exists(filePath))
        QSKIP("Local co_elite2 model not present.");

    const DecodedModel decoded = CmpLoader::loadModel(filePath);
    QVERIFY2(decoded.isValid(), "Decoded co_elite2 model is invalid");
    QCOMPARE(decoded.format, NativeModelFormat::Cmp);
    QVERIFY(!decoded.vmeshRefs.isEmpty());

    int refsWithResolvedPart = 0;
    for (const auto &ref : decoded.vmeshRefs) {
        if (!ref.partName.trimmed().isEmpty())
            ++refsWithResolvedPart;
    }
    QCOMPARE(refsWithResolvedPart, decoded.vmeshRefs.size());

    int combinedHintCount = 0;
    for (const auto &hint : decoded.cmpTransformHints) {
        if (hint.hasCombinedTranslation || hint.hasCombinedRotation)
            ++combinedHintCount;
    }
    QVERIFY(combinedHintCount > 0);

    const auto bounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
    QVERIFY(bounds.valid);
    QVERIFY(qIsFinite(bounds.radius()));
    QVERIFY(bounds.radius() > 0.0f);
}

void TestSimpleShipModel::cvStarflierMaterialSignatureSnapshot()
{
    const QString filePath = QStringLiteral(
        "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp");

    if (!QFileInfo::exists(filePath))
        QSKIP("Local CV Starflier model not present.");

    const DecodedModel decoded = CmpLoader::loadModel(filePath);
    QStringList partSnapshot;
    QStringList materialSnapshot;

    std::function<void(const ModelNode &, int)> walk = [&](const ModelNode &node, int depth) {
        if (depth > 0)
            partSnapshot.append(QStringLiteral("%1|%2|%3")
                                    .arg(node.name)
                                    .arg(node.meshes.size())
                                    .arg(node.children.size()));
        for (const auto &mesh : node.meshes) {
            if (!mesh.materialName.isEmpty()) {
                materialSnapshot.append(QStringLiteral("%1|%2|%3")
                                            .arg(node.name)
                                            .arg(mesh.vertices.size())
                                            .arg(mesh.materialName));
            }
        }
        for (const auto &child : node.children)
            walk(child, depth + 1);
    };

    walk(decoded.rootNode, 0);

    const QStringList expectedPartSnapshot = {
        QStringLiteral("Root|5|4"),
        QStringLiteral("Part_baydoor01_lod1|2|0"),
        QStringLiteral("Part_baydoor02_lod1|2|0"),
        QStringLiteral("Part_engine_lod1|4|0"),
        QStringLiteral("Part_glass_lod1|3|0"),
        QStringLiteral("Part_port_wing_lod1|4|0"),
        QStringLiteral("Part_star_wing_lod1|4|0"),
    };
    const QStringList expectedMaterialSnapshot = {};

    QStringList sortedPartSnapshot = partSnapshot;
    QStringList sortedExpectedPartSnapshot = expectedPartSnapshot;
    QStringList sortedMaterialSnapshot = materialSnapshot;
    QStringList sortedExpectedMaterialSnapshot = expectedMaterialSnapshot;

    sortedPartSnapshot.sort();
    sortedExpectedPartSnapshot.sort();
    sortedMaterialSnapshot.sort();
    sortedExpectedMaterialSnapshot.sort();

    QCOMPARE(sortedPartSnapshot, sortedExpectedPartSnapshot);
    QCOMPARE(sortedMaterialSnapshot, sortedExpectedMaterialSnapshot);
}

QTEST_GUILESS_MAIN(TestSimpleShipModel)
#include "test_SimpleShipModel.moc"
