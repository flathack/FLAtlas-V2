#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QDebug>
#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestStationSolarModel : public QObject {
    Q_OBJECT
private slots:
    void loadStationLargeALodCmp()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/station_large_a_lod.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local station_large_a_lod model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        int resolvedRefs = 0;
        int directRefs = 0;
        int familyFallbackRefs = 0;
        int meshCount = 0;
        QStringList materialSnapshot;
        for (const auto &ref : decoded.vmeshRefs) {
            if (!ref.matchedSourceName.isEmpty())
                ++resolvedRefs;
            if (!ref.debugHint.isEmpty() && ref.debugHint.startsWith(QStringLiteral("direct:")))
                ++directRefs;
            if (ref.usedStructuredFamilyFallback)
                ++familyFallbackRefs;
        }

        QStringList childSnapshot;
        std::function<void(const ModelNode &, int)> walk = [&](const ModelNode &node, int depth) {
            if (depth == 1)
                childSnapshot.append(QStringLiteral("%1|%2|%3")
                                         .arg(node.name)
                                         .arg(node.meshes.size())
                                         .arg(node.children.size()));
            meshCount += node.meshes.size();
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

        const QStringList expectedChildSnapshot = {
            QStringLiteral("Main_lod1021120110645.3db|12|0"),
            QStringLiteral("Part_dock1_poly_lod1|2|1"),
            QStringLiteral("Part_dock2_poly_lod1|2|1"),
            QStringLiteral("Part_dock3_poly_lod1|2|1"),
            QStringLiteral("Part_dock4_poly_lod1|12|1"),
        };
        const QStringList expectedMaterialSnapshot = {
            QStringLiteral("Main_lod1021120110645.3db|2719|material_296598637"),
            QStringLiteral("Main_lod1021120110645.3db|4|material_206954468"),
            QStringLiteral("Main_lod1021120110645.3db|4|material_206954468"),
            QStringLiteral("Main_lod1021120110645.3db|529|material_456291904"),
            QStringLiteral("Main_lod1021120110645.3db|602|material_66826125"),
            QStringLiteral("Main_lod1021120110645.3db|607|material_66826125"),
            QStringLiteral("Main_lod1021120110645.3db|764|material_456291904"),
            QStringLiteral("Main_lod1021120110645.3db|908|material_296598637"),
            QStringLiteral("Part_dock1_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock1_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock2_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock2_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock3_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock3_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock4_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock4_door_lod1|4|material_135985979"),
            QStringLiteral("Part_dock4_poly_lod1|4|material_135985979"),
            QStringLiteral("Part_dock4_poly_lod1|4|material_135985979"),
            QStringLiteral("Part_dock4_poly_lod1|4|material_135985979"),
            QStringLiteral("Part_dock4_poly_lod1|4|material_135985979"),
            QStringLiteral("Part_dock4_poly_lod1|4|material_206954468"),
            QStringLiteral("Part_dock4_poly_lod1|607|material_66826125"),
        };

        QStringList sortedChildSnapshot = childSnapshot;
        QStringList sortedExpectedChildSnapshot = expectedChildSnapshot;
        QStringList sortedMaterialSnapshot = materialSnapshot;
        QStringList sortedExpectedMaterialSnapshot = expectedMaterialSnapshot;
        sortedChildSnapshot.sort();
        sortedExpectedChildSnapshot.sort();
        sortedMaterialSnapshot.sort();
        sortedExpectedMaterialSnapshot.sort();

        QVERIFY2(decoded.isValid(), "Decoded station solar model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(resolvedRefs, decoded.vmeshRefs.size());
        QCOMPARE(directRefs, 20);
        QCOMPARE(familyFallbackRefs, 0);
        QCOMPARE(meshCount, 38);
        QCOMPARE(sortedChildSnapshot, sortedExpectedChildSnapshot);
        QCOMPARE(sortedMaterialSnapshot, sortedExpectedMaterialSnapshot);
        QVERIFY(!childSnapshot.contains(QStringLiteral("Root|0|0")));
    }
};

QTEST_GUILESS_MAIN(TestStationSolarModel)
#include "test_StationSolarModel.moc"