#include <QtTest>
#include "rendering/preview/ModelCache.h"

using namespace flatlas::rendering;
using namespace flatlas::infrastructure;

class TestModelCache : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        ModelCache::instance().clear();
        ModelCache::instance().setMaxSize(128);
    }

    void emptyCache()
    {
        QVERIFY(!ModelCache::instance().contains("test.cmp"));
        QCOMPARE(ModelCache::instance().size(), 0);
    }

    void insertAndContains()
    {
        ModelNode node;
        node.name = "test";
        ModelCache::instance().insert("test.cmp", node);
        QVERIFY(ModelCache::instance().contains("test.cmp"));
        QCOMPARE(ModelCache::instance().size(), 1);
    }

    void getReturnsInserted()
    {
        ModelNode node;
        node.name = "mymodel";
        ModelCache::instance().insert("model.3db", node);

        auto retrieved = ModelCache::instance().get("model.3db");
        QCOMPARE(retrieved.name, QStringLiteral("mymodel"));
    }

    void getReturnsEmptyForMissing()
    {
        auto retrieved = ModelCache::instance().get("nonexistent.cmp");
        QVERIFY(retrieved.name.isEmpty());
        QVERIFY(retrieved.meshes.isEmpty());
    }

    void clearEmptiesCache()
    {
        ModelNode node;
        node.name = "test";
        ModelCache::instance().insert("a.cmp", node);
        ModelCache::instance().insert("b.cmp", node);
        QCOMPARE(ModelCache::instance().size(), 2);

        ModelCache::instance().clear();
        QCOMPARE(ModelCache::instance().size(), 0);
        QVERIFY(!ModelCache::instance().contains("a.cmp"));
    }

    void lruEviction()
    {
        ModelCache::instance().setMaxSize(3);

        for (int i = 0; i < 3; ++i) {
            ModelNode node;
            node.name = QString::number(i);
            ModelCache::instance().insert(QString("model%1.cmp").arg(i), node);
        }
        QCOMPARE(ModelCache::instance().size(), 3);

        // Insert a 4th item → should evict model0 (oldest)
        ModelNode newNode;
        newNode.name = "new";
        ModelCache::instance().insert("model3.cmp", newNode);

        QCOMPARE(ModelCache::instance().size(), 3);
        QVERIFY(!ModelCache::instance().contains("model0.cmp")); // evicted
        QVERIFY(ModelCache::instance().contains("model1.cmp"));
        QVERIFY(ModelCache::instance().contains("model2.cmp"));
        QVERIFY(ModelCache::instance().contains("model3.cmp"));
    }

    void lruAccessUpdatesOrder()
    {
        ModelCache::instance().setMaxSize(3);

        ModelNode n;
        n.name = "a";
        ModelCache::instance().insert("a.cmp", n);
        n.name = "b";
        ModelCache::instance().insert("b.cmp", n);
        n.name = "c";
        ModelCache::instance().insert("c.cmp", n);

        // Access "a" to move it to most recent
        ModelCache::instance().get("a.cmp");

        // Insert new item → should evict "b" (now oldest), not "a"
        n.name = "d";
        ModelCache::instance().insert("d.cmp", n);

        QVERIFY(ModelCache::instance().contains("a.cmp"));
        QVERIFY(!ModelCache::instance().contains("b.cmp")); // evicted
        QVERIFY(ModelCache::instance().contains("c.cmp"));
        QVERIFY(ModelCache::instance().contains("d.cmp"));
    }

    void updateExistingEntry()
    {
        ModelNode n;
        n.name = "v1";
        ModelCache::instance().insert("model.cmp", n);

        n.name = "v2";
        ModelCache::instance().insert("model.cmp", n);

        QCOMPARE(ModelCache::instance().size(), 1);
        auto retrieved = ModelCache::instance().get("model.cmp");
        QCOMPARE(retrieved.name, QStringLiteral("v2"));
    }
};

QTEST_GUILESS_MAIN(TestModelCache)
#include "test_ModelCache.moc"
