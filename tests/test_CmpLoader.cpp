#include <QtTest>
#include "infrastructure/io/CmpLoader.h"

using namespace flatlas::infrastructure;

class TestCmpLoader : public QObject {
    Q_OBJECT
private slots:
    void loadNonExistentFile()
    {
        auto model = CmpLoader::loadCmp(QStringLiteral("nonexistent.cmp"));
        QVERIFY(model.meshes.isEmpty());
        QVERIFY(model.children.isEmpty());
    }

    void load3dbNonExistentFile()
    {
        auto model = CmpLoader::load3db(QStringLiteral("nonexistent.3db"));
        QVERIFY(model.meshes.isEmpty());
    }

    void parseUtfEmpty()
    {
        QByteArray empty;
        auto root = CmpLoader::parseUtf(empty);
        QVERIFY(!root || root->children.isEmpty());
    }

    void parseUtfTooShort()
    {
        QByteArray tooShort("UTF 12345");
        auto root = CmpLoader::parseUtf(tooShort);
        QVERIFY(!root || root->children.isEmpty());
    }

    void parseUtfSignatureCheck()
    {
        // Valid size but wrong signature
        QByteArray bad(56, '\0');
        bad[0] = 'X'; bad[1] = 'T'; bad[2] = 'F'; bad[3] = ' ';
        auto root = CmpLoader::parseUtf(bad);
        QVERIFY(!root || root->children.isEmpty());
    }

    void findNodeEmptyPathReturnsRoot()
    {
        auto root = std::make_shared<UtfNode>();
        root->name = "root";
        auto found = CmpLoader::findNode(root, QString());
        // Empty path returns the root node itself
        QVERIFY(found);
        QCOMPARE(found->name, QStringLiteral("root"));
    }

    void findNodeSimplePath()
    {
        auto root = std::make_shared<UtfNode>();
        root->name = "root";
        auto child = std::make_shared<UtfNode>();
        child->name = "child";
        root->children.append(child);

        auto found = CmpLoader::findNode(root, QStringLiteral("child"));
        QVERIFY(found);
        QCOMPARE(found->name, QStringLiteral("child"));
    }

    void findNodeNestedPath()
    {
        auto root = std::make_shared<UtfNode>();
        root->name = "root";
        auto level1 = std::make_shared<UtfNode>();
        level1->name = "VMeshLibrary";
        auto level2 = std::make_shared<UtfNode>();
        level2->name = "mesh_data";
        level1->children.append(level2);
        root->children.append(level1);

        auto found = CmpLoader::findNode(root, QStringLiteral("VMeshLibrary\\mesh_data"));
        QVERIFY(found);
        QCOMPARE(found->name, QStringLiteral("mesh_data"));
    }

    void findNodeCaseInsensitive()
    {
        auto root = std::make_shared<UtfNode>();
        root->name = "root";
        auto child = std::make_shared<UtfNode>();
        child->name = "VMeshPart";
        root->children.append(child);

        auto found = CmpLoader::findNode(root, QStringLiteral("vmeshpart"));
        QVERIFY(found);
    }

    void findNodeNotFound()
    {
        auto root = std::make_shared<UtfNode>();
        root->name = "root";

        auto found = CmpLoader::findNode(root, QStringLiteral("nonexistent"));
        QVERIFY(!found);
    }

};

QTEST_GUILESS_MAIN(TestCmpLoader)
#include "test_CmpLoader.moc"
