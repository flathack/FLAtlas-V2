// test_ModelPreview.cpp – Phase 16 Model Preview & Character Preview tests
// Qt3D tests use the OpenGL renderer to avoid RHI plugin issues in test envs.

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "rendering/preview/ModelPreview.h"
#include "rendering/preview/CharacterPreview.h"
#include "rendering/preview/ModelCache.h"

using namespace flatlas::rendering;

class TestModelPreview : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void testModelPreviewCreation();
    void testModelPreviewLoadNonexistent();
    void testCharacterPreviewCreation();
    void testCharacterPreviewScanEmptyDir();
    void testModelCacheBasic();
};

void TestModelPreview::initTestCase()
{
    // Force OpenGL renderer – the RHI renderer plugin may not load in all envs.
    qputenv("QT3D_RENDERER", "opengl");
}

void TestModelPreview::testModelPreviewCreation()
{
    ModelPreview dlg;
    QCOMPARE(dlg.hasModel(), false);
    QVERIFY(dlg.filePath().isEmpty());
}

void TestModelPreview::testModelPreviewLoadNonexistent()
{
    ModelPreview dlg;
    dlg.loadModel(QStringLiteral("nonexistent_file.cmp"));
    QCOMPARE(dlg.hasModel(), false);
}

void TestModelPreview::testCharacterPreviewCreation()
{
    CharacterPreview dlg;
    QVERIFY(dlg.currentBody().isEmpty());
    QVERIFY(dlg.availableBodies().isEmpty());
    QVERIFY(dlg.availableHeads().isEmpty());
}

void TestModelPreview::testCharacterPreviewScanEmptyDir()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    CharacterPreview dlg;
    dlg.setCharacterDir(dir.path());

    QCOMPARE(dlg.availableBodies().size(), 0);
    QCOMPARE(dlg.availableHeads().size(), 0);
}

void TestModelPreview::testModelCacheBasic()
{
    auto &cache = ModelCache::instance();
    cache.clear();

    QCOMPARE(cache.size(), 0);
    QCOMPARE(cache.contains(QStringLiteral("foo")), false);

    cache.setMaxSize(10);
    QCOMPARE(cache.maxSize(), 10);
}

QTEST_MAIN(TestModelPreview)
#include "test_ModelPreview.moc"
