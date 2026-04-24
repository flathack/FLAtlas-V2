#include <QtTest>

#include "rendering/preview/ModelCache.h"

using namespace flatlas::rendering;
using namespace flatlas::infrastructure;

class TestModelCache : public QObject {
    Q_OBJECT
private:
    static DecodedModel makeModel(const QString &name, const QString &sourcePath = {})
    {
        DecodedModel model;
        model.sourcePath = sourcePath;
        model.rootNode.name = name;
        return model;
    }

private slots:
    void init()
    {
        ModelCache::instance().clear();
        ModelCache::instance().setMaxSize(128);
    }

    void emptyCache()
    {
        QVERIFY(!ModelCache::instance().contains(QStringLiteral("test.cmp")));
        QCOMPARE(ModelCache::instance().size(), 0);
    }

    void insertAndContains()
    {
        ModelCache::instance().insert(QStringLiteral("test.cmp"),
                                      makeModel(QStringLiteral("test"), QStringLiteral("test")));
        QVERIFY(ModelCache::instance().contains(QStringLiteral("test.cmp")));
        QCOMPARE(ModelCache::instance().size(), 1);
    }

    void getReturnsInserted()
    {
        ModelCache::instance().insert(QStringLiteral("model.3db"),
                                      makeModel(QStringLiteral("mymodel"),
                                                QStringLiteral("mymodel.3db")));

        const auto retrieved = ModelCache::instance().get(QStringLiteral("model.3db"));
        QCOMPARE(retrieved.rootNode.name, QStringLiteral("mymodel"));
        QCOMPARE(retrieved.sourcePath, QStringLiteral("mymodel.3db"));
    }

    void getReturnsEmptyForMissing()
    {
        const auto retrieved = ModelCache::instance().get(QStringLiteral("nonexistent.cmp"));
        QVERIFY(retrieved.sourcePath.isEmpty());
        QVERIFY(retrieved.rootNode.name.isEmpty());
        QVERIFY(retrieved.rootNode.meshes.isEmpty());
        QVERIFY(retrieved.rootNode.children.isEmpty());
    }

    void clearEmptiesCache()
    {
        const DecodedModel model = makeModel(QStringLiteral("test"));
        ModelCache::instance().insert(QStringLiteral("a.cmp"), model);
        ModelCache::instance().insert(QStringLiteral("b.cmp"), model);
        QCOMPARE(ModelCache::instance().size(), 2);

        ModelCache::instance().clear();
        QCOMPARE(ModelCache::instance().size(), 0);
        QVERIFY(!ModelCache::instance().contains(QStringLiteral("a.cmp")));
    }

    void lruEviction()
    {
        ModelCache::instance().setMaxSize(3);

        for (int i = 0; i < 3; ++i) {
            ModelCache::instance().insert(QStringLiteral("model%1.cmp").arg(i),
                                          makeModel(QString::number(i),
                                                    QStringLiteral("model%1.cmp").arg(i)));
        }
        QCOMPARE(ModelCache::instance().size(), 3);

        ModelCache::instance().insert(QStringLiteral("model3.cmp"),
                                      makeModel(QStringLiteral("new"),
                                                QStringLiteral("model3.cmp")));

        QCOMPARE(ModelCache::instance().size(), 3);
        QVERIFY(!ModelCache::instance().contains(QStringLiteral("model0.cmp")));
        QVERIFY(ModelCache::instance().contains(QStringLiteral("model1.cmp")));
        QVERIFY(ModelCache::instance().contains(QStringLiteral("model2.cmp")));
        QVERIFY(ModelCache::instance().contains(QStringLiteral("model3.cmp")));
    }

    void lruAccessUpdatesOrder()
    {
        ModelCache::instance().setMaxSize(3);

        ModelCache::instance().insert(QStringLiteral("a.cmp"), makeModel(QStringLiteral("a")));
        ModelCache::instance().insert(QStringLiteral("b.cmp"), makeModel(QStringLiteral("b")));
        ModelCache::instance().insert(QStringLiteral("c.cmp"), makeModel(QStringLiteral("c")));

        ModelCache::instance().get(QStringLiteral("a.cmp"));
        ModelCache::instance().insert(QStringLiteral("d.cmp"), makeModel(QStringLiteral("d")));

        QVERIFY(ModelCache::instance().contains(QStringLiteral("a.cmp")));
        QVERIFY(!ModelCache::instance().contains(QStringLiteral("b.cmp")));
        QVERIFY(ModelCache::instance().contains(QStringLiteral("c.cmp")));
        QVERIFY(ModelCache::instance().contains(QStringLiteral("d.cmp")));
    }

    void updateExistingEntry()
    {
        ModelCache::instance().insert(QStringLiteral("model.cmp"),
                                      makeModel(QStringLiteral("v1"),
                                                QStringLiteral("model-v1.cmp")));
        ModelCache::instance().insert(QStringLiteral("model.cmp"),
                                      makeModel(QStringLiteral("v2"),
                                                QStringLiteral("model-v2.cmp")));

        QCOMPARE(ModelCache::instance().size(), 1);
        const auto retrieved = ModelCache::instance().get(QStringLiteral("model.cmp"));
        QCOMPARE(retrieved.rootNode.name, QStringLiteral("v2"));
        QCOMPARE(retrieved.sourcePath, QStringLiteral("model-v2.cmp"));
    }
};

QTEST_GUILESS_MAIN(TestModelCache)
#include "test_ModelCache.moc"
