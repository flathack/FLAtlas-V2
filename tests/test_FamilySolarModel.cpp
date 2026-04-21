#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QDebug>
#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestFamilySolarModel : public QObject {
    Q_OBJECT
private slots:
    void spaceDomeRefAndMeshSnapshot();
    void loadSpaceDomeCmp()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/MISC/space_dome.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local space_dome model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        qDebug() << "warnings" << decoded.warnings;
        qDebug() << "parts" << decoded.parts.size()
                 << "vmeshRefs" << decoded.vmeshRefs.size()
                 << "vmeshBlocks" << decoded.vmeshDataBlocks.size();
        for (const auto &ref : decoded.vmeshRefs) {
            qDebug() << "ref path" << ref.nodePath
                     << "meshRef" << ref.meshDataReference
                     << "source" << ref.matchedSourceName
                     << "resolution" << ref.resolutionHint
                     << "usedFamily" << ref.usedStructuredFamilyFallback
                     << "debug" << ref.debugHint;
        }
        QVERIFY2(decoded.isValid(), "Decoded family solar model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);

        int meshCount = 0;
        int childMeshNodes = 0;
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

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

void TestFamilySolarModel::spaceDomeRefAndMeshSnapshot()
{
    const QString filePath = QStringLiteral(
        "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/MISC/space_dome.cmp");

    if (!QFileInfo::exists(filePath))
        QSKIP("Local space_dome model not present.");

    const DecodedModel decoded = CmpLoader::loadModel(filePath);
    QStringList refSnapshot;
    for (const auto &ref : decoded.vmeshRefs) {
        refSnapshot.append(QStringLiteral("%1|%2|%3|%4")
                               .arg(ref.nodePath,
                                    ref.matchedSourceName,
                                    ref.resolutionHint,
                                    ref.debugHint));
    }

    QStringList meshSnapshot;
    const auto childWithMeshes = std::find_if(decoded.rootNode.children.cbegin(),
                                              decoded.rootNode.children.cend(),
                                              [](const ModelNode &node) {
                                                  return !node.meshes.isEmpty();
                                              });
    QVERIFY(childWithMeshes != decoded.rootNode.children.cend());
    for (const auto &mesh : childWithMeshes->meshes) {
        meshSnapshot.append(QStringLiteral("%1|%2|%3")
                                .arg(mesh.vertices.size())
                                .arg(mesh.indices.size())
                                .arg(mesh.debugHint));
    }

    const QStringList expectedRefSnapshot = {
        QStringLiteral("\\/space_dome_lod1021003093018.3db/MultiLevel/Level2/VMeshPart/VMeshRef|data.solar.misc.space_dome.lod2-112.vms|crc-or-fallback|direct:structured-header:data.solar.misc.space_dome.lod2-112.vms/structured-single-block@0"),
        QStringLiteral("\\/space_dome_lod1021003093018.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.misc.space_dome.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.misc.space_dome.lod1-112.vms/structured-single-block@0"),
        QStringLiteral("\\/space_dome_lod1021003093018.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.misc.space_dome.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.misc.space_dome.lod0-112.vms/structured-single-block@0"),
        QStringLiteral("\\/dome_lod1021003093018.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.misc.space_dome.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.misc.space_dome.lod1-112.vms/structured-single-block@0"),
        QStringLiteral("\\/dome_lod1021003093018.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.misc.space_dome.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.misc.space_dome.lod0-112.vms/structured-single-block@0"),
    };
    const QStringList expectedMeshSnapshot = {
        QStringLiteral("78|102|direct:structured-header:data.solar.misc.space_dome.lod2-112.vms/structured-single-block@0"),
        QStringLiteral("174|132|direct:structured-header:data.solar.misc.space_dome.lod2-112.vms/structured-single-block@0"),
        QStringLiteral("78|102|direct:structured-header:data.solar.misc.space_dome.lod1-112.vms/structured-single-block@0"),
        QStringLiteral("238|144|direct:structured-header:data.solar.misc.space_dome.lod1-112.vms/structured-single-block@0"),
        QStringLiteral("84|120|direct:structured-header:data.solar.misc.space_dome.lod0-112.vms/structured-single-block@0"),
        QStringLiteral("278|294|direct:structured-header:data.solar.misc.space_dome.lod0-112.vms/structured-single-block@0"),
        QStringLiteral("300|480|direct:structured-header:data.solar.misc.space_dome.lod1-112.vms/structured-single-block@0"),
        QStringLiteral("340|480|direct:structured-header:data.solar.misc.space_dome.lod0-112.vms/structured-single-block@0"),
    };

    QCOMPARE(refSnapshot, expectedRefSnapshot);
    QCOMPARE(meshSnapshot, expectedMeshSnapshot);
}

QTEST_GUILESS_MAIN(TestFamilySolarModel)
#include "test_FamilySolarModel.moc"
