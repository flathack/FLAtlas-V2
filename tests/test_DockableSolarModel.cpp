#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QDebug>
#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestDockableSolarModel : public QObject {
    Q_OBJECT
private slots:
    void dockingRingX2RefResolutionSnapshot();
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
        for (int i = 0; i < decoded.vmeshDataBlocks.size(); ++i) {
            const auto &block = decoded.vmeshDataBlocks.at(i);
            qDebug() << "block" << i
                     << "source" << block.sourceName
                     << "family" << block.familyKey
                     << "kind" << block.headerHint.structureKind;
        }
        int resolvedRefs = 0;
        QHash<QString, int> resolvedSources;
        for (const auto &ref : decoded.vmeshRefs) {
            qDebug() << "ref path" << ref.nodePath
                     << "meshRef" << ref.meshDataReference
                     << "source" << ref.matchedSourceName
                     << "resolution" << ref.resolutionHint
                     << "usedFamily" << ref.usedStructuredFamilyFallback
                     << "debug" << ref.debugHint;
            if (!ref.matchedSourceName.isEmpty()) {
                ++resolvedRefs;
                resolvedSources[ref.matchedSourceName] += 1;
            }
        }
        QVERIFY2(decoded.isValid(), "Decoded dockable solar model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(resolvedRefs, decoded.vmeshRefs.size());
        QCOMPARE(resolvedSources.value(QStringLiteral("data.solar.dockable.docking_ringx2_lod.lod1-212.vms")), 3);
        QCOMPARE(resolvedSources.value(QStringLiteral("data.solar.dockable.docking_ringx2_lod.lod1-112.vms")), 4);
        QCOMPARE(resolvedSources.value(QStringLiteral("data.solar.dockable.docking_ringx2_lod.lod0-212.vms")), 3);
        QCOMPARE(resolvedSources.value(QStringLiteral("data.solar.dockable.docking_ringx2_lod.lod0-112.vms")), 4);

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

void TestDockableSolarModel::dockingRingX2RefResolutionSnapshot()
{
    const QString filePath = QStringLiteral(
        "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp");

    if (!QFileInfo::exists(filePath))
        QSKIP("Local docking_ringx2_lod model not present.");

    const DecodedModel decoded = CmpLoader::loadModel(filePath);
    QVERIFY(decoded.vmeshRefs.size() == 14);

    QStringList snapshot;
    for (const auto &ref : decoded.vmeshRefs) {
        snapshot.append(QStringLiteral("%1|%2|%3")
                            .arg(ref.nodePath,
                                 ref.matchedSourceName,
                                 ref.resolutionHint));
    }

    const QStringList expected = {
        QStringLiteral("\\/Dock_lod1021031102238.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod1-212.vms|crc-or-fallback"),
        QStringLiteral("\\/Dock_lod1021031102238.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod0-212.vms|crc-or-fallback"),
        QStringLiteral("\\/dock_b_lod1021031102238.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod1-212.vms|crc-or-fallback"),
        QStringLiteral("\\/dock_b_lod1021031102238.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod0-212.vms|crc-or-fallback"),
        QStringLiteral("\\/dock_a_lod1021031102238.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod1-212.vms|crc-or-fallback"),
        QStringLiteral("\\/dock_a_lod1021031102238.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod0-212.vms|crc-or-fallback"),
        QStringLiteral("\\/dock2b_lod1021031102238.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod1-112.vms|crc-or-fallback"),
        QStringLiteral("\\/dock2b_lod1021031102238.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod0-112.vms|crc-or-fallback"),
        QStringLiteral("\\/dock2a_lod1021031102238.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod1-112.vms|crc-or-fallback"),
        QStringLiteral("\\/dock2a_lod1021031102238.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod0-112.vms|crc-or-fallback"),
        QStringLiteral("\\/dock1b_lod1021031102238.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod1-112.vms|crc-or-fallback"),
        QStringLiteral("\\/dock1b_lod1021031102238.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod0-112.vms|crc-or-fallback"),
        QStringLiteral("\\/dock1a_lod1021031102238.3db/MultiLevel/Level1/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod1-112.vms|crc-or-fallback"),
        QStringLiteral("\\/dock1a_lod1021031102238.3db/MultiLevel/Level0/VMeshPart/VMeshRef|data.solar.dockable.docking_ringx2_lod.lod0-112.vms|crc-or-fallback"),
    };

    QCOMPARE(snapshot, expected);
}

QTEST_GUILESS_MAIN(TestDockableSolarModel)
#include "test_DockableSolarModel.moc"
