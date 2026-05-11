// test_ModelViewerPageTradeLane.cpp - reproduces loading TLR_lod.3db through the actual ModelViewerPage flow

#include <QtTest/QtTest>

#include "rendering/preview/ModelViewerPage.h"
#include "rendering/view3d/ModelViewport3D.h"
#include "core/EditingContext.h"

#include <QDir>
#include <QFileInfo>
#include <QTreeWidget>

class TestModelViewerPageTradeLane : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void testLoadTradeLaneThroughViewerPage();
};

void TestModelViewerPageTradeLane::initTestCase()
{
    qputenv("QT3D_RENDERER", "opengl");
    const QString testModPath = QStringLiteral("C:/Users/steve/Github/FL-Installationen/TESTMOD1");
    if (!QFileInfo::exists(QDir(testModPath).filePath(QStringLiteral("DATA/SOLAR/DOCKABLE/TLR_lod.3db")))
        && !QFileInfo::exists(QDir(testModPath).filePath(QStringLiteral("DATA/SOLAR/dockable/TLR_lod.3db")))
        && !QFileInfo::exists(QDir(testModPath).filePath(QStringLiteral("DATA/SOLAR/dockable/tlr_lod.3db")))) {
        QSKIP("TESTMOD1 Freelancer model fixture is not available in this environment.");
    }

    flatlas::core::ModProfile profile;
    profile.id = QStringLiteral("testmod1");
    profile.name = QStringLiteral("TESTMOD1");
    profile.mode = QStringLiteral("direct");
    profile.directPath = testModPath;
    auto &context = flatlas::core::EditingContext::instance();
    context.addProfile(profile);
    QVERIFY(context.setEditingProfile(profile.id));
}

void TestModelViewerPageTradeLane::testLoadTradeLaneThroughViewerPage()
{
    flatlas::rendering::ModelViewerPage page;
    page.resize(1400, 900);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page, 5000));

    auto *tree = page.findChild<QTreeWidget *>();
    QVERIFY(tree != nullptr);

    QTreeWidgetItem *targetItem = nullptr;
    auto findTradeLaneItem = [&]() {
        QTreeWidgetItemIterator it(tree);
        while (*it) {
            auto *item = *it;
            const QString modelPath = item->data(0, Qt::UserRole).toString().toLower();
            if (modelPath.endsWith(QStringLiteral("/solar/dockable/tlr_lod.3db")) ||
                modelPath.endsWith(QStringLiteral("\\solar\\dockable\\tlr_lod.3db")) ||
                modelPath.endsWith(QStringLiteral("tlr_lod.3db"))) {
                targetItem = item;
                return true;
            }
            ++it;
        }
        return false;
    };

    QTRY_VERIFY_WITH_TIMEOUT(findTradeLaneItem(), 10000);
    QVERIFY2(targetItem != nullptr, "Trade lane ring model was not found in the viewer tree");

    const QString modelPath = targetItem->data(0, Qt::UserRole).toString();
    QVERIFY(page.loadModelPath(modelPath));

    auto *viewport = page.findChild<flatlas::rendering::ModelViewport3D *>();
    QVERIFY(viewport != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(viewport->hasModel(), 10000);
}

QTEST_MAIN(TestModelViewerPageTradeLane)
#include "test_ModelViewerPageTradeLane.moc"
