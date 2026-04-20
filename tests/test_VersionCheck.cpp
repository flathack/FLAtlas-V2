// test_VersionCheck.cpp – Phase 22 Auto-Updater tests

#include <QtTest/QtTest>
#include "tools/UpdateChecker.h"
#include "tools/UpdateDownloader.h"

using namespace flatlas::tools;

class TestVersionCheck : public QObject {
    Q_OBJECT
private slots:
    void testCurrentVersion();
    void testRepoInfo();
    void testLatestReleaseUrl();
    void testIsNewerVersionBasic();
    void testIsNewerVersionWithPrefix();
    void testIsNewerVersionSame();
    void testIsNewerVersionOlder();
    void testDownloaderInitialState();
    void testDownloaderCancelNoOp();
};

void TestVersionCheck::testCurrentVersion()
{
    const QString ver = UpdateChecker::currentVersion();
    QVERIFY(!ver.isEmpty());
    // Should be a valid version string
    QVERIFY(ver.contains(QLatin1Char('.')));
}

void TestVersionCheck::testRepoInfo()
{
    QVERIFY(!UpdateChecker::repoOwner().isEmpty());
    QVERIFY(!UpdateChecker::repoName().isEmpty());
}

void TestVersionCheck::testLatestReleaseUrl()
{
    const QUrl url = UpdateChecker::latestReleaseUrl();
    QVERIFY(url.isValid());
    QVERIFY(url.toString().contains(QStringLiteral("api.github.com")));
    QVERIFY(url.toString().contains(UpdateChecker::repoOwner()));
    QVERIFY(url.toString().contains(UpdateChecker::repoName()));
}

void TestVersionCheck::testIsNewerVersionBasic()
{
    QVERIFY(UpdateChecker::isNewerVersion("1.0.0", "2.0.0"));
    QVERIFY(UpdateChecker::isNewerVersion("1.0.0", "1.1.0"));
    QVERIFY(UpdateChecker::isNewerVersion("1.0.0", "1.0.1"));
}

void TestVersionCheck::testIsNewerVersionWithPrefix()
{
    QVERIFY(UpdateChecker::isNewerVersion("v1.0.0", "v2.0.0"));
    QVERIFY(UpdateChecker::isNewerVersion("1.0.0", "v2.0.0"));
    QVERIFY(UpdateChecker::isNewerVersion("v1.0.0", "2.0.0"));
}

void TestVersionCheck::testIsNewerVersionSame()
{
    QVERIFY(!UpdateChecker::isNewerVersion("1.0.0", "1.0.0"));
    QVERIFY(!UpdateChecker::isNewerVersion("v1.0.0", "v1.0.0"));
}

void TestVersionCheck::testIsNewerVersionOlder()
{
    QVERIFY(!UpdateChecker::isNewerVersion("2.0.0", "1.0.0"));
    QVERIFY(!UpdateChecker::isNewerVersion("1.1.0", "1.0.0"));
}

void TestVersionCheck::testDownloaderInitialState()
{
    UpdateDownloader dl;
    QVERIFY(!dl.isDownloading());
}

void TestVersionCheck::testDownloaderCancelNoOp()
{
    UpdateDownloader dl;
    dl.cancel(); // should not crash
    QVERIFY(!dl.isDownloading());
}

QTEST_MAIN(TestVersionCheck)
#include "test_VersionCheck.moc"
