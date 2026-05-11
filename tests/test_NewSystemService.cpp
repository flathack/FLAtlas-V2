#include <QtTest/QtTest>

#include "core/Config.h"
#include "domain/UniverseData.h"
#include "editors/universe/NewSystemService.h"
#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/freelancer/ResourceDllWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

using namespace flatlas::domain;
using namespace flatlas::editors;
using namespace flatlas::infrastructure;

class TestNewSystemService : public QObject
{
    Q_OBJECT

private slots:
    void createSystemUsesConfiguredIdsTargetDll();
};

void TestNewSystemService::createSystemUsesConfiguredIdsTargetDll()
{
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("EXE")));
    QVERIFY(root.mkpath(QStringLiteral("DATA/UNIVERSE")));

    QString templatePath;
    const QStringList candidates = {
        QStringLiteral("C:/Users/steve/Github/FL-Installationen/_FL Fresh Install-englisch/EXE/nameresources.dll"),
        QStringLiteral("C:/Users/steve/Github/FL-Installationen/_FL Fresh Install-deutsch/EXE/nameresources.dll"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            templatePath = candidate;
            break;
        }
    }
    if (templatePath.isEmpty())
        QSKIP("Freelancer name resource DLL template is not available in this environment.");
    QVERIFY(QFile::copy(templatePath, root.filePath(QStringLiteral("EXE/NameResources.dll"))));

    {
        QFile freelancerIni(root.filePath(QStringLiteral("EXE/freelancer.ini")));
        QVERIFY(freelancerIni.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&freelancerIni);
        out << "[Resources]\n"
            << "DLL = NameResources.dll\n";
    }

    const QString universePath = root.filePath(QStringLiteral("DATA/UNIVERSE/universe.ini"));
    {
        QFile universeIni(universePath);
        QVERIFY(universeIni.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&universeIni);
        out << "[Universe]\n";
    }

    auto &config = flatlas::core::Config::instance();
    const QJsonObject previousConfig = config.data();
    config.setString(QStringLiteral("idsCreationTargetDll"), QStringLiteral("remres.dll"));

    UniverseData universe;
    NewSystemRequest request;
    request.systemName = QStringLiteral("Cortez");
    request.systemPrefix = QStringLiteral("Cx");

    CreatedSystemResult result;
    QString errorMessage;
    const bool created = NewSystemService::createSystem(universePath,
                                                        universe,
                                                        request,
                                                        QPointF(4.0, 7.0),
                                                        &result,
                                                        &errorMessage);
    config.setData(previousConfig);

    QVERIFY2(created, qPrintable(errorMessage));
    QVERIFY(result.systemInfo.stridName > 0);

    const QStringList resourceDlls =
        ResourceDllWriter::resourceDllsFromFreelancerIni(root.filePath(QStringLiteral("EXE/freelancer.ini")));
    QCOMPARE(resourceDlls, QStringList({QStringLiteral("NameResources.dll"), QStringLiteral("remres.dll")}));
    QVERIFY(!resourceDlls.contains(ResourceDllWriter::preferredFlatlasDllName()));

    const int slot = (result.systemInfo.stridName >> 16) & 0xFFFF;
    QCOMPARE(slot, 2);

    const IdsDataset reloaded = IdsDataService::loadFromGameRoot(dir.path());
    bool foundCreatedName = false;
    for (const auto &entry : reloaded.entries) {
        if (entry.globalId == result.systemInfo.stridName) {
            foundCreatedName = true;
            QCOMPARE(entry.dllName, QStringLiteral("remres.dll"));
            QCOMPARE(entry.stringValue, QStringLiteral("Cortez"));
            break;
        }
    }
    QVERIFY(foundCreatedName);
#else
    QSKIP("Resource DLL writing is only supported on Windows.");
#endif
}

QTEST_GUILESS_MAIN(TestNewSystemService)
#include "test_NewSystemService.moc"
