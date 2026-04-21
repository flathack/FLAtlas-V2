#include <QtTest>

#include "infrastructure/io/CmpLoader.h"
#include "rendering/view3d/ModelGeometryBuilder.h"

#include <QFileInfo>
#include <QDebug>

using namespace flatlas::infrastructure;

class TestTradeLaneModel : public QObject {
    Q_OBJECT
private slots:
    void loadTradeLaneModel()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/TLR_lod.3db");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local Trade Lane model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        qDebug() << "warnings" << decoded.warnings;
        QVERIFY2(decoded.isValid(), "Decoded model is invalid");

        int meshCount = 0;
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
                         << "indices" << mesh.indices.size();
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

        const auto sceneBounds = flatlas::rendering::ModelGeometryBuilder::boundsForNode(decoded.rootNode);
        QVERIFY(sceneBounds.valid);
        QVERIFY(qIsFinite(sceneBounds.radius()));
        QVERIFY(sceneBounds.radius() > 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestTradeLaneModel)
#include "test_TradeLaneModel.moc"
