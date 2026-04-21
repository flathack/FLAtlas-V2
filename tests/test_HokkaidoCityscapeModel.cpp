#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestHokkaidoCityscapeModel : public QObject {
    Q_OBJECT
private slots:
    void loadHokkaidoCityscapeCmp()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/BASES/KUSARI/ku_01_hokkaido_cityscape.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local ku_01_hokkaido_cityscape model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);

        int resolvedRefs = 0;
        int directRefs = 0;
        int familyFallbackRefs = 0;
        for (const auto &ref : decoded.vmeshRefs) {
            if (!ref.matchedSourceName.isEmpty())
                ++resolvedRefs;
            if (!ref.debugHint.isEmpty() && ref.debugHint.startsWith(QStringLiteral("direct:")))
                ++directRefs;
            if (ref.usedStructuredFamilyFallback)
                ++familyFallbackRefs;
        }

        int meshCount = 0;
        QStringList childSnapshot;
        std::function<void(const ModelNode &, int)> walk = [&](const ModelNode &node, int depth) {
            if (depth == 1) {
                childSnapshot.append(QStringLiteral("%1|%2|%3|%4|%5|%6")
                                         .arg(node.name)
                                         .arg(node.meshes.size())
                                         .arg(node.children.size())
                                         .arg(node.origin.x(), 0, 'f', 3)
                                         .arg(node.origin.y(), 0, 'f', 3)
                                         .arg(node.origin.z(), 0, 'f', 3));
            }
            meshCount += node.meshes.size();
            for (const auto &child : node.children)
                walk(child, depth + 1);
        };
        walk(decoded.rootNode, 0);

        const QStringList expectedChildSnapshot = {
            QStringLiteral("Part_Building01|2|0|0.000|0.000|0.000"),
            QStringLiteral("Part_Building04|2|0|0.000|0.000|0.000"),
            QStringLiteral("Part_Building05|3|0|0.000|0.000|0.000"),
            QStringLiteral("Part_Fade Mid|3|0|0.000|0.000|0.000"),
            QStringLiteral("Part_Flag Thing02|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_Hokkaido Scape|3|0|0.000|0.000|0.000"),
            QStringLiteral("Part_SpotLight01_C|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_SpotLight01|5|0|0.000|0.000|0.000"),
            QStringLiteral("Part_banner1|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_banner2|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_banner3|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_banner4|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_bigfin|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_billboard|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_fade_front|2|0|0.000|0.000|0.000"),
            QStringLiteral("Part_newban2|1|0|0.000|0.000|0.000"),
            QStringLiteral("Part_newban|2|0|0.000|0.000|0.000"),
            QStringLiteral("Part_trans_rails|2|0|0.000|0.000|0.000"),
            QStringLiteral("Root|2|32|0.000|0.000|0.000"),
        };

        QStringList sortedChildSnapshot = childSnapshot;
        QStringList sortedExpectedChildSnapshot = expectedChildSnapshot;
        sortedChildSnapshot.sort();
        sortedExpectedChildSnapshot.sort();

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY2(decoded.isValid(), "Decoded Hokkaido cityscape model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(decoded.parts.size(), 51);
        QCOMPARE(decoded.vmeshRefs.size(), 51);
        QCOMPARE(decoded.vmeshDataBlocks.size(), 2);
        QCOMPARE(decoded.materialReferences.size(), 14);
        QCOMPARE(decoded.previewMaterialBindings.size(), 51);
        QCOMPARE(resolvedRefs, 51);
        QCOMPARE(directRefs, 51);
        QCOMPARE(familyFallbackRefs, 0);
        QCOMPARE(meshCount, 67);
        QCOMPARE(decoded.rootNode.name, QStringLiteral("ku_01_hokkaido_cityscape.cmp"));
        QCOMPARE(decoded.rootNode.children.size(), 19);
        QCOMPARE(sortedChildSnapshot, sortedExpectedChildSnapshot);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestHokkaidoCityscapeModel)
#include "test_HokkaidoCityscapeModel.moc"
