#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QFileInfo>
#include <QDebug>

using namespace flatlas::infrastructure;

class TestTradeLaneModel : public QObject {
    Q_OBJECT
private slots:
    void tradeLaneMeshSignatureSnapshot();
    void loadTradeLaneModel()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/TLR_lod.3db");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local Trade Lane model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        qDebug() << "warnings" << decoded.warnings;
        QVERIFY2(decoded.isValid(), "Decoded model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Db3);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(decoded.rootNode.name, QStringLiteral("MultiLevel"));
        QCOMPARE(decoded.rootNode.children.size(), 0);

        int meshCount = 0;
        QStringList meshSignatures;
        std::function<void(const ModelNode&, int)> walk = [&](const ModelNode &node, int depth) {
            qDebug() << "node depth" << depth
                     << "name" << node.name
                     << "meshes" << node.meshes.size()
                     << "children" << node.children.size()
                     << "origin" << node.origin
                     << "rotation scalar" << node.rotation.scalar();
            for (const auto &mesh : node.meshes) {
                ++meshCount;
                qDebug() << "mesh vertices" << mesh.vertices.size()
                         << "indices" << mesh.indices.size()
                         << "debug" << mesh.debugHint;
                meshSignatures.append(QStringLiteral("%1x%2").arg(mesh.vertices.size()).arg(mesh.indices.size()));
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
        const QStringList expectedSignatures = {
            QStringLiteral("512x912"),
            QStringLiteral("840x1680"),
            QStringLiteral("1640x3456"),
            QStringLiteral("2456x6336"),
        };
        QCOMPARE(meshSignatures, expectedSignatures);

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

void TestTradeLaneModel::tradeLaneMeshSignatureSnapshot()
{
    const QString filePath = QStringLiteral(
        "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/TLR_lod.3db");

    if (!QFileInfo::exists(filePath))
        QSKIP("Local Trade Lane model not present.");

    const DecodedModel decoded = CmpLoader::loadModel(filePath);
    QStringList snapshot;
    for (const auto &mesh : decoded.rootNode.meshes)
        snapshot.append(QStringLiteral("%1|%2|%3").arg(mesh.vertices.size()).arg(mesh.indices.size()).arg(mesh.debugHint));

    const QStringList expected = {
        QStringLiteral("512|912|direct:structured-header:data.solar.dockable.tlr_lod.lod3-112.vms/structured-mesh-headers"),
        QStringLiteral("840|1680|direct:structured-header:data.solar.dockable.tlr_lod.lod2-112.vms/structured-mesh-headers"),
        QStringLiteral("1640|3456|direct:structured-header:data.solar.dockable.tlr_lod.lod1-112.vms/structured-mesh-headers"),
        QStringLiteral("2456|6336|direct:structured-header:data.solar.dockable.tlr_lod.lod0-112.vms/structured-mesh-headers"),
    };

    QCOMPARE(snapshot, expected);
}

QTEST_GUILESS_MAIN(TestTradeLaneModel)
#include "test_TradeLaneModel.moc"
