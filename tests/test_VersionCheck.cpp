// test_VersionCheck.cpp – Phase 22 Auto-Updater tests

#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonObject>
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
    void testCurrentArchitecture();
    void testSelectBestAssetPrefersMatchingArchitecture();
    void testSelectBestAssetFallsBackToGenericWindowsZip();
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

void TestVersionCheck::testCurrentArchitecture()
{
    const QString arch = UpdateChecker::currentArchitecture();
    QVERIFY(arch == QStringLiteral("x64")
            || arch == QStringLiteral("x86")
            || arch == QStringLiteral("arm64"));
}

void TestVersionCheck::testSelectBestAssetPrefersMatchingArchitecture()
{
    QJsonArray assets;
    assets.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("FLAtlas-V2-win32.zip")},
        {QStringLiteral("browser_download_url"), QStringLiteral("https://example.invalid/win32.zip")}
    });
    assets.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("FLAtlas-V2-win-x64.zip")},
        {QStringLiteral("browser_download_url"), QStringLiteral("https://example.invalid/x64.zip")}
    });

    const UpdateAsset asset = UpdateChecker::selectBestAsset(assets, QStringLiteral("x64"));
    QCOMPARE(asset.name, QStringLiteral("FLAtlas-V2-win-x64.zip"));
    QCOMPARE(asset.packageType, QStringLiteral("zip"));
}

void TestVersionCheck::testSelectBestAssetFallsBackToGenericWindowsZip()
{
    QJsonArray assets;
    assets.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("FLAtlas-V2-Windows.zip")},
        {QStringLiteral("browser_download_url"), QStringLiteral("https://example.invalid/windows.zip")}
    });
    assets.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("FLAtlas-V2-linux-x64.zip")},
        {QStringLiteral("browser_download_url"), QStringLiteral("https://example.invalid/linux.zip")}
    });

    const UpdateAsset asset = UpdateChecker::selectBestAsset(assets, QStringLiteral("arm64"));
    QCOMPARE(asset.name, QStringLiteral("FLAtlas-V2-Windows.zip"));
    QCOMPARE(asset.packageType, QStringLiteral("zip"));
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
