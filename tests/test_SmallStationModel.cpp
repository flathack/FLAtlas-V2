#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QFileInfo>

using namespace flatlas::infrastructure;

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
        QCOMPARE(decoded.cmpTransformHints.size(), 9);

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

        QVERIFY(decoded.rootNode.children.size() > 0);

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestSmallStationModel)
#include "test_SmallStationModel.moc"
