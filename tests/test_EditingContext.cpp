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
        QVERIFY(ctx.addProfile(p));

        QCOMPARE(ctx.profiles().size(), 1);
        QCOMPARE(ctx.profiles().first().name, QStringLiteral("TestMod"));
        QVERIFY(!ctx.profiles().first().id.isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    void testRejectDuplicateProfileByPath()
    {
        auto &ctx = flatlas::core::EditingContext::instance();
        QSignalSpy spy(&ctx, &flatlas::core::EditingContext::profilesChanged);

        flatlas::core::ModProfile duplicate;
        duplicate.name = QStringLiteral("Duplicate TestMod");
        duplicate.mode = QStringLiteral("direct");
        duplicate.directPath = QDir(m_tmpDir.path()).filePath(QStringLiteral("."));

        QVERIFY(!ctx.addProfile(duplicate));
        QCOMPARE(ctx.profiles().size(), 1);
        QCOMPARE(spy.count(), 0);
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

    void testRestoreRemovesDuplicatePaths()
    {
        auto &cfg = flatlas::core::Config::instance();
        QJsonArray arr;

        QJsonObject p1;
        p1[QStringLiteral("id")] = QStringLiteral("dup-1");
        p1[QStringLiteral("name")] = QStringLiteral("One");
        p1[QStringLiteral("mode")] = QStringLiteral("direct");
        p1[QStringLiteral("direct_path")] = m_tmpDir.path();
        arr.append(p1);

        QJsonObject p2;
        p2[QStringLiteral("id")] = QStringLiteral("dup-2");
        p2[QStringLiteral("name")] = QStringLiteral("Two");
        p2[QStringLiteral("mode")] = QStringLiteral("direct");
        p2[QStringLiteral("direct_path")] = QDir(m_tmpDir.path()).filePath(QStringLiteral("."));
        arr.append(p2);

        cfg.setJsonArray(QStringLiteral("modmanager.profiles"), arr);
        cfg.setString(QStringLiteral("modmanager.editing_id"), QStringLiteral("dup-2"));
        QVERIFY(cfg.save());

        auto &ctx = flatlas::core::EditingContext::instance();
        ctx.restore();

        QCOMPARE(ctx.profiles().size(), 1);
        QCOMPARE(ctx.profiles().first().id, QStringLiteral("dup-1"));
        QVERIFY(ctx.editingProfileId().isEmpty());
    }

private:
    QTemporaryDir m_tmpDir;
};

QTEST_MAIN(TestEditingContext)
#include "test_EditingContext.moc"
