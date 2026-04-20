// tests/test_EditingContext.cpp – Tests für EditingContext
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QSignalSpy>
#include "core/Config.h"
#include "core/EditingContext.h"

class TestEditingContext : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        m_tmpDir.setAutoRemove(true);
        // Load config from a temp path so we don't pollute real config
        flatlas::core::Config::instance().load(m_tmpDir.filePath(QStringLiteral("config.json")));
    }

    void testAddProfile()
    {
        auto &ctx = flatlas::core::EditingContext::instance();
        QSignalSpy spy(&ctx, &flatlas::core::EditingContext::profilesChanged);

        flatlas::core::ModProfile p;
        p.name = QStringLiteral("TestMod");
        p.mode = QStringLiteral("direct");
        p.directPath = m_tmpDir.path();
        ctx.addProfile(p);

        QCOMPARE(ctx.profiles().size(), 1);
        QCOMPARE(ctx.profiles().first().name, QStringLiteral("TestMod"));
        QVERIFY(!ctx.profiles().first().id.isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    void testSetEditingProfile()
    {
        auto &ctx = flatlas::core::EditingContext::instance();
        QVERIFY(!ctx.profiles().isEmpty());

        QString id = ctx.profiles().first().id;
        QSignalSpy spy(&ctx, &flatlas::core::EditingContext::contextChanged);

        bool ok = ctx.setEditingProfile(id);
        QVERIFY(ok);
        QCOMPARE(ctx.editingProfileId(), id);
        QVERIFY(ctx.hasContext());
        QCOMPARE(ctx.primaryGamePath(), m_tmpDir.path());
        QCOMPARE(spy.count(), 1);
    }

    void testPersistAndRestore()
    {
        auto &ctx = flatlas::core::EditingContext::instance();
        QString savedId = ctx.editingProfileId();
        int savedCount = ctx.profiles().size();
        QSignalSpy profilesSpy(&ctx, &flatlas::core::EditingContext::profilesChanged);
        QSignalSpy contextSpy(&ctx, &flatlas::core::EditingContext::contextChanged);

        // Simulate restart: restore from config
        ctx.restore();

        QCOMPARE(ctx.profiles().size(), savedCount);
        QCOMPARE(ctx.editingProfileId(), savedId);
        QVERIFY(ctx.hasContext());
        QCOMPARE(profilesSpy.count(), 1);
        QCOMPARE(contextSpy.count(), 1);
        QCOMPARE(contextSpy.takeFirst().at(0).toString(), savedId);
    }

    void testClearContext()
    {
        auto &ctx = flatlas::core::EditingContext::instance();
        QSignalSpy spy(&ctx, &flatlas::core::EditingContext::contextChanged);

        ctx.clearEditingProfile();
        QVERIFY(!ctx.hasContext());
        QVERIFY(ctx.editingProfileId().isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    void testRemoveProfile()
    {
        auto &ctx = flatlas::core::EditingContext::instance();
        QVERIFY(!ctx.profiles().isEmpty());

        QString id = ctx.profiles().first().id;
        ctx.removeProfile(id);
        QCOMPARE(ctx.profiles().size(), 0);
    }

    void testModProfileJson()
    {
        flatlas::core::ModProfile p;
        p.id = QStringLiteral("abc123");
        p.name = QStringLiteral("TestFL");
        p.mode = QStringLiteral("direct");
        p.directPath = QStringLiteral("C:/Games/Freelancer");

        auto json = p.toJson();
        auto p2 = flatlas::core::ModProfile::fromJson(json);
        QCOMPARE(p2.id, p.id);
        QCOMPARE(p2.name, p.name);
        QCOMPARE(p2.mode, p.mode);
        QCOMPARE(p2.directPath, p.directPath);
        QCOMPARE(p2.sourcePath(), p.directPath);
    }

    void testRepoProfile()
    {
        flatlas::core::ModProfile p;
        p.id = QStringLiteral("repo1");
        p.name = QStringLiteral("Crossfire");
        p.mode = QStringLiteral("repo");
        p.repoRoot = QStringLiteral("C:/Mods");
        p.repoFolder = QStringLiteral("Crossfire");

        QCOMPARE(p.sourcePath(), QStringLiteral("C:/Mods/Crossfire"));
    }

private:
    QTemporaryDir m_tmpDir;
};

QTEST_MAIN(TestEditingContext)
#include "test_EditingContext.moc"
