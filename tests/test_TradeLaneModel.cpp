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
            QStringLiteral("336x624"),
            QStringLiteral("400x720"),
            QStringLiteral("440x78"),
            QStringLiteral("832x1728"),
            QStringLiteral("1248x3360"),
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
        QStringLiteral("336|624|direct:structured-header:data.solar.dockable.tlr_lod.lod3-112.vms/structured-single-block@0"),
        QStringLiteral("400|720|direct:structured-header:data.solar.dockable.tlr_lod.lod2-112.vms/structured-single-block@0"),
        QStringLiteral("440|78|direct:structured-header:data.solar.dockable.tlr_lod.lod2-112.vms/structured-single-block@0"),
        QStringLiteral("832|1728|direct:structured-header:data.solar.dockable.tlr_lod.lod1-112.vms/structured-single-block@0"),
        QStringLiteral("1248|3360|direct:structured-header:data.solar.dockable.tlr_lod.lod0-112.vms/structured-single-block@0"),
    };

    QCOMPARE(snapshot, expected);
}

QTEST_GUILESS_MAIN(TestTradeLaneModel)
#include "test_TradeLaneModel.moc"
