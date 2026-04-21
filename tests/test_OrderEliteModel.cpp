#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestOrderEliteModel : public QObject {
    Q_OBJECT
private slots:
    void loadOrderEliteCmp()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SHIPS/ORDER/OR_ELITE/or_elite.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local OR_ELITE model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        int resolvedRefs = 0;
        int directRefs = 0;
        int noTextureBindings = 0;
        QStringList bindingSnapshot;
        for (const auto &ref : decoded.vmeshRefs) {
            if (!ref.matchedSourceName.isEmpty())
                ++resolvedRefs;
            if (!ref.debugHint.isEmpty() && ref.debugHint.startsWith(QStringLiteral("direct:")))
                ++directRefs;
        }
        for (const auto &binding : decoded.previewMaterialBindings) {
            if (binding.matchHint == QStringLiteral("no-texture-reference"))
                ++noTextureBindings;
            bindingSnapshot.append(QStringLiteral("%1|%2|%3|%4|%5")
                                      .arg(binding.modelName,
                                           binding.levelName,
                                           binding.partName,
                                           QString::number(binding.groupStart),
                                           QString::number(binding.groupCount) + QStringLiteral("|") + binding.matchHint));
        }

        QVERIFY2(decoded.isValid(), "Decoded Order Elite model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(decoded.parts.size(), 9);
        QCOMPARE(decoded.vmeshRefs.size(), 25);
        QCOMPARE(decoded.vmeshDataBlocks.size(), 3);
        QCOMPARE(decoded.materialReferences.size(), 0);
        QCOMPARE(decoded.previewMaterialBindings.size(), decoded.vmeshRefs.size());
        QCOMPARE(resolvedRefs, decoded.vmeshRefs.size());
        QCOMPARE(directRefs, 25);
        QCOMPARE(noTextureBindings, decoded.previewMaterialBindings.size());

        int meshCount = 0;
        QStringList childSnapshot;
        QStringList materialSnapshot;
        std::function<void(const ModelNode &, int)> walk = [&](const ModelNode &node, int depth) {
            if (depth == 1)
                childSnapshot.append(QStringLiteral("%1|%2|%3")
                                         .arg(node.name)
                                         .arg(node.meshes.size())
                                         .arg(node.children.size()));
            meshCount += node.meshes.size();
            for (const auto &mesh : node.meshes) {
                QVERIFY(mesh.vertices.size() > 0);
                QVERIFY(mesh.indices.size() > 0);
                if (!mesh.materialName.isEmpty()) {
                    materialSnapshot.append(QStringLiteral("%1|%2|%3|%4")
                                                .arg(node.name)
                                                .arg(mesh.vertices.size())
                                                .arg(mesh.materialName)
                                                .arg(mesh.matchHint));
                }
                const auto bounds = flatlas::rendering::ModelGeometryBuilder::boundsForMesh(mesh);
                QVERIFY(bounds.valid);
            }
            for (const auto &child : node.children)
                walk(child, depth + 1);
        };
        walk(decoded.rootNode, 0);

        const QStringList expectedChildSnapshot = {
            QStringLiteral("Or_elite_lod1021102155951.3db|3|0"),
            QStringLiteral("Part_Or_Engine01_lod1|3|0"),
            QStringLiteral("Part_Or_Engine02_lod1|3|0"),
            QStringLiteral("Part_Or_Engine03_lod1|3|0"),
            QStringLiteral("Part_Or_glass_lod1|3|0"),
            QStringLiteral("Part_Or_port_wing_lod1|3|0"),
            QStringLiteral("Part_Or_star_wing_lod1|11|0"),
            QStringLiteral("Root|0|2"),
        };
        const QStringList expectedMaterialSnapshot = {
            QStringLiteral("Part_Or_glass_lod1|61|material_354002449|no-texture-reference"),
            QStringLiteral("Part_Or_glass_lod1|127|material_354002449|no-texture-reference"),
            QStringLiteral("Part_Or_glass_lod1|164|material_354002449|no-texture-reference"),
            QStringLiteral("Part_Or_star_wing_lod1|164|material_354002449|no-texture-reference"),
        };
        const QStringList expectedBindingSnapshot = {
            QStringLiteral("Or_Engine01_lod1021102155951.3db|Level0|Part_Or_Engine01_lod1|4|2|no-texture-reference"),
            QStringLiteral("Or_Engine01_lod1021102155951.3db|Level1|Part_Or_Engine01_lod1|3|2|no-texture-reference"),
            QStringLiteral("Or_Engine01_lod1021102155951.3db|Level2|Part_Or_Engine01_lod1|2|2|no-texture-reference"),
            QStringLiteral("Or_Engine02_lod1021102155951.3db|Level0|Part_Or_Engine02_lod1|7|3|no-texture-reference"),
            QStringLiteral("Or_Engine02_lod1021102155951.3db|Level1|Part_Or_Engine02_lod1|6|2|no-texture-reference"),
            QStringLiteral("Or_Engine02_lod1021102155951.3db|Level2|Part_Or_Engine02_lod1|5|2|no-texture-reference"),
            QStringLiteral("Or_Engine03_lod1021102155951.3db|Level0|Part_Or_Engine03_lod1|0|4|no-texture-reference"),
            QStringLiteral("Or_Engine03_lod1021102155951.3db|Level1|Part_Or_Engine03_lod1|0|3|no-texture-reference"),
            QStringLiteral("Or_Engine03_lod1021102155951.3db|Level2|Part_Or_Engine03_lod1|0|2|no-texture-reference"),
            QStringLiteral("Or_elite_lod1021102155951.3db|Level0|Root|10|5|no-texture-reference"),
            QStringLiteral("Or_elite_lod1021102155951.3db|Level1|Root|8|5|no-texture-reference"),
            QStringLiteral("Or_elite_lod1021102155951.3db|Level2|Root|7|2|no-texture-reference"),
            QStringLiteral("Or_glass_lod1021102155951.3db|Level0|Part_Or_glass_lod1|6|1|no-texture-reference"),
            QStringLiteral("Or_glass_lod1021102155951.3db|Level1|Part_Or_glass_lod1|5|1|no-texture-reference"),
            QStringLiteral("Or_glass_lod1021102155951.3db|Level2|Part_Or_glass_lod1|4|1|no-texture-reference"),
            QStringLiteral("Or_port_wing_lod1021102155951.3db|Level0|Part_Or_port_wing_lod1|15|3|no-texture-reference"),
            QStringLiteral("Or_port_wing_lod1021102155951.3db|Level1|Part_Or_port_wing_lod1|13|2|no-texture-reference"),
            QStringLiteral("Or_port_wing_lod1021102155951.3db|Level2|Part_Or_port_wing_lod1|9|2|no-texture-reference"),
            QStringLiteral("Or_star_wing_lod1021102155951.3db|Level0|Part_Or_star_wing_lod1|20|3|no-texture-reference"),
            QStringLiteral("Or_star_wing_lod1021102155951.3db|Level1|Part_Or_star_wing_lod1|17|2|no-texture-reference"),
            QStringLiteral("Or_star_wing_lod1021102155951.3db|Level2|Part_Or_star_wing_lod1|11|2|no-texture-reference"),
            QStringLiteral("baydoor01_lod1021102155951.3db|Level0|Part_baydoor01_lod1|19|1|no-texture-reference"),
            QStringLiteral("baydoor01_lod1021102155951.3db|Level1|Part_baydoor01_lod1|16|1|no-texture-reference"),
            QStringLiteral("baydoor02_lod1021102155951.3db|Level0|Part_baydoor02_lod1|18|1|no-texture-reference"),
            QStringLiteral("baydoor02_lod1021102155951.3db|Level1|Part_baydoor02_lod1|15|1|no-texture-reference"),
        };

        QStringList sortedChildSnapshot = childSnapshot;
        QStringList sortedExpectedChildSnapshot = expectedChildSnapshot;
        QStringList sortedMaterialSnapshot = materialSnapshot;
        QStringList sortedExpectedMaterialSnapshot = expectedMaterialSnapshot;
        QStringList sortedBindingSnapshot = bindingSnapshot;
        QStringList sortedExpectedBindingSnapshot = expectedBindingSnapshot;
        sortedChildSnapshot.sort();
        sortedExpectedChildSnapshot.sort();
        sortedMaterialSnapshot.sort();
        sortedExpectedMaterialSnapshot.sort();
        sortedBindingSnapshot.sort();
        sortedExpectedBindingSnapshot.sort();

        QCOMPARE(meshCount, 33);
        QCOMPARE(sortedChildSnapshot, sortedExpectedChildSnapshot);
        QCOMPARE(sortedMaterialSnapshot, sortedExpectedMaterialSnapshot);
        QCOMPARE(sortedBindingSnapshot, sortedExpectedBindingSnapshot);
    }
};

QTEST_GUILESS_MAIN(TestOrderEliteModel)
#include "test_OrderEliteModel.moc"
