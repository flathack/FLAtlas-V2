// test_ModelViewerPageTradeLane.cpp - reproduces loading TLR_lod.3db through the actual ModelViewerPage flow

#include <QtTest/QtTest>

#include "rendering/preview/ModelViewerPage.h"
#include "core/EditingContext.h"

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
    flatlas::core::ModProfile profile;
    profile.id = QStringLiteral("testmod1");
    profile.name = QStringLiteral("TESTMOD1");
    profile.mode = QStringLiteral("direct");
    profile.directPath = QStringLiteral("C:/Users/steve/Github/FL-Installationen/TESTMOD1");
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
    QTreeWidgetItemIterator it(tree);
    while (*it) {
        auto *item = *it;
        const QString modelPath = item->data(0, Qt::UserRole).toString().toLower();
        if (modelPath.endsWith(QStringLiteral("/solar/dockable/tlr_lod.3db")) ||
            modelPath.endsWith(QStringLiteral("\\solar\\dockable\\tlr_lod.3db")) ||
            modelPath.endsWith(QStringLiteral("tlr_lod.3db"))) {
            targetItem = item;
            break;
        }
        ++it;
    }

    QVERIFY2(targetItem != nullptr, "Trade lane ring model was not found in the viewer tree");

    tree->setCurrentItem(targetItem);
    tree->scrollToItem(targetItem);
    QTest::mouseDClick(tree->viewport(),
                       Qt::LeftButton,
                       Qt::NoModifier,
                       tree->visualItemRect(targetItem).center());

    QTest::qWait(500);
}

QTEST_MAIN(TestModelViewerPageTradeLane)
#include "test_ModelViewerPageTradeLane.moc"
