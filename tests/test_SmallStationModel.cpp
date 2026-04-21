#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QFileInfo>

using namespace flatlas::infrastructure;

namespace {

const ModelNode *findNodeByName(const ModelNode &node, const QString &name)
{
    if (node.name == name)
        return &node;
    for (const auto &child : node.children) {
        if (const auto *match = findNodeByName(child, name))
            return match;
    }
    return nullptr;
}

QStringList meshSignatures(const ModelNode &node)
{
    QStringList signatures;
    for (const auto &mesh : node.meshes) {
        signatures.append(QStringLiteral("%1/%2|%3")
                              .arg(mesh.vertices.size())
                              .arg(mesh.indices.size())
                              .arg(mesh.debugHint));
    }
    return signatures;
}

QStringList childNames(const ModelNode &node)
{
    QStringList names;
    for (const auto &child : node.children)
        names.append(child.name);
    return names;
}

} // namespace

class TestSmallStationModel : public QObject {
    Q_OBJECT
private slots:
    void loadSmallStationCmp()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/station_small_b_lod.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local station_small_b_lod model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        QVERIFY2(decoded.isValid(), "Decoded small station model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(decoded.parts.size(), 9);
        QCOMPARE(decoded.vmeshRefs.size(), 25);
        QCOMPARE(decoded.vmeshDataBlocks.size(), 8);
        QCOMPARE(decoded.cmpTransformHints.size(), 12);

        QStringList refSnapshot;
        int familyFallbackRefs = 0;
        for (const auto &ref : decoded.vmeshRefs) {
            if (ref.usedStructuredFamilyFallback)
                ++familyFallbackRefs;
            refSnapshot.append(QStringLiteral("%1|%2|%3|%4")
                                   .arg(ref.nodePath,
                                        ref.matchedSourceName,
                                        ref.resolutionHint,
                                        ref.debugHint));
        }

        const QStringList expectedRefs = {
            QStringLiteral("\\/ring_lod1021120120523.3db/MultiLevel/Level2/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod2-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod2-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/ring_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/ring_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/Main_lod1021120120523.3db/MultiLevel/Level4/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod4-212.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod4-212.vms/structured-mesh-headers"),
            QStringLiteral("\\/Main_lod1021120120523.3db/MultiLevel/Level3/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod3-212.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod3-212.vms/structured-mesh-headers"),
            QStringLiteral("\\/Main_lod1021120120523.3db/MultiLevel/Level2/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod2-212.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod2-212.vms/structured-mesh-headers"),
            QStringLiteral("\\/Main_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/Main_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock3_poly_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock3_poly_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock3_door_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock3_door_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock2_poly_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock2_poly_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock2_door_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock2_door_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock1_poly_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock1_poly_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock1_door_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/dock1_door_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/cntrl_twr_lod1021120120523.3db/MultiLevel/Level4/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod4-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod4-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/cntrl_twr_lod1021120120523.3db/MultiLevel/Level3/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod3-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod3-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/cntrl_twr_lod1021120120523.3db/MultiLevel/Level2/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod2-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod2-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/cntrl_twr_lod1021120120523.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod1-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
            QStringLiteral("\\/cntrl_twr_lod1021120120523.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.station_small_b_lod.lod0-112.vms|crc-or-fallback|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
        };

        QCOMPARE(familyFallbackRefs, 0);
        QCOMPARE(refSnapshot, expectedRefs);

        QCOMPARE(decoded.rootNode.children.size(), 5);
        QCOMPARE(childNames(decoded.rootNode),
                 QStringList({QStringLiteral("Root"),
                              QStringLiteral("Part_dock1_poly_lod1"),
                              QStringLiteral("Part_dock2_poly_lod1"),
                              QStringLiteral("Part_dock3_poly_lod1"),
                              QStringLiteral("Main_lod1021120120523.3db")}));

        const auto *root = findNodeByName(decoded.rootNode, QStringLiteral("Root"));
        QVERIFY(root);
        QCOMPARE(root->children.size(), 2);
        QCOMPARE(childNames(*root),
                 QStringList({QStringLiteral("Part_cntrl_twr_lod1"),
                              QStringLiteral("Part_ring_lod1")}));

        const auto *mainNode = findNodeByName(decoded.rootNode, QStringLiteral("Main_lod1021120120523.3db"));
        QVERIFY(mainNode);
        QCOMPARE(meshSignatures(*mainNode),
                 QStringList({
                     QStringLiteral("525/657|direct:structured-header:data.solar.dockable.station_small_b_lod.lod4-212.vms/structured-mesh-headers"),
                     QStringLiteral("944/1110|direct:structured-header:data.solar.dockable.station_small_b_lod.lod3-212.vms/structured-mesh-headers"),
                     QStringLiteral("1854/2562|direct:structured-header:data.solar.dockable.station_small_b_lod.lod2-212.vms/structured-mesh-headers"),
                     QStringLiteral("3130/4509|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
                     QStringLiteral("4734/6348|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
                 }));

        const auto *controlTower = findNodeByName(decoded.rootNode, QStringLiteral("Part_cntrl_twr_lod1"));
        QVERIFY(controlTower);
        QCOMPARE(meshSignatures(*controlTower),
                 QStringList({
                     QStringLiteral("37/66|direct:structured-header:data.solar.dockable.station_small_b_lod.lod4-112.vms/structured-mesh-headers"),
                     QStringLiteral("78/180|direct:structured-header:data.solar.dockable.station_small_b_lod.lod3-112.vms/structured-mesh-headers"),
                     QStringLiteral("269/453|direct:structured-header:data.solar.dockable.station_small_b_lod.lod2-112.vms/structured-mesh-headers"),
                     QStringLiteral("622/1065|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers"),
                     QStringLiteral("683/1305|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers"),
                 }));

        const auto *ring = findNodeByName(decoded.rootNode, QStringLiteral("Part_ring_lod1"));
        QVERIFY(ring);
        const QStringList ringMeshes = meshSignatures(*ring);
        QVERIFY(ringMeshes.contains(QStringLiteral("156/162|direct:structured-header:data.solar.dockable.station_small_b_lod.lod2-112.vms/structured-mesh-headers")));
        QCOMPARE(ringMeshes.count(QStringLiteral("300/306|direct:structured-header:data.solar.dockable.station_small_b_lod.lod1-112.vms/structured-mesh-headers")), 1);
        QCOMPARE(ringMeshes.count(QStringLiteral("300/306|direct:structured-header:data.solar.dockable.station_small_b_lod.lod0-112.vms/structured-mesh-headers")), 1);

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestSmallStationModel)
#include "test_SmallStationModel.moc"