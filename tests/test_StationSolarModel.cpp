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

        QStringList sortedChildSnapshot = childSnapshot;
        QStringList sortedExpectedChildSnapshot = expectedChildSnapshot;
        sortedChildSnapshot.sort();
        sortedExpectedChildSnapshot.sort();

        QVERIFY2(decoded.isValid(), "Decoded station solar model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(resolvedRefs, decoded.vmeshRefs.size());
        QCOMPARE(directRefs, 20);
        QCOMPARE(familyFallbackRefs, 0);
        QCOMPARE(meshCount, 38);
        QCOMPARE(sortedChildSnapshot, sortedExpectedChildSnapshot);
        QVERIFY(!childSnapshot.contains(QStringLiteral("Root|0|0")));
    }
};

QTEST_GUILESS_MAIN(TestStationSolarModel)
#include "test_StationSolarModel.moc"