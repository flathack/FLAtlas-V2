#include <QtTest/QtTest>

#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/freelancer/ResourceDllWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

using namespace flatlas::infrastructure;

class TestIdsDataService : public QObject {
    Q_OBJECT

private slots:
    void testScanMissingAndReferences();
    void testAssignFieldValue();
    void testCreateEntriesPersistInFlatlasDll();
};

void TestIdsDataService::testScanMissingAndReferences()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("EXE")));
    QVERIFY(root.mkpath(QStringLiteral("DATA/UNIVERSE")));

    {
        QFile freelancerIni(root.filePath(QStringLiteral("EXE/freelancer.ini")));
        QVERIFY(freelancerIni.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&freelancerIni);
        out << "[Resources]\n"
            << "DLL = resources.dll\n";
    }

    {
        QFile ini(root.filePath(QStringLiteral("DATA/UNIVERSE/test.ini")));
        QVERIFY(ini.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&ini);
        out << "[System]\n"
            << "nickname = li01\n"
            << "strid_name = 196609\n"
            << "ids_info = 0\n\n"
            << "[Object]\n"
            << "nickname = obj_alpha\n"
            << "archetype = station\n"
            << "base = li01_01_base\n"
            << "ids_name = 0\n"
            << "ids_info = 131072\n\n"
            << "[Zone]\n"
            << "nickname = zone_beta\n"
            << "ids_name = 0\n"
            << "ids_info = 0\n\n"
            << "[Object]\n"
            << "nickname = buoy_gamma\n"
            << "archetype = nav_buoy\n\n"
            << "[Room]\n"
            << "nickname = li01_bar\n"
            << "ids_name = 0\n";
    }

    const IdsDataset dataset = IdsDataService::loadFromGameRoot(dir.path());
    QCOMPARE(dataset.references.size(), 2);

    bool foundSystemStridReference = false;
    bool foundObjectInfoReference = false;
    for (const auto &reference : dataset.references) {
        if (reference.globalId == 196609 && reference.usageType == IdsUsageType::StridName)
            foundSystemStridReference = true;
        if (reference.globalId == 131072 && reference.usageType == IdsUsageType::IdsInfo)
            foundObjectInfoReference = true;
    }
    QVERIFY(foundSystemStridReference);
    QVERIFY(foundObjectInfoReference);

    QCOMPARE(dataset.missingAssignments.size(), 3);

    int systemInfoMissingCount = 0;
    int objectNameMissingCount = 0;
    int roomNameMissingCount = 0;
    for (const auto &record : dataset.missingAssignments) {
        if (record.sectionName.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0
            && record.usageType == IdsUsageType::IdsInfo) {
            ++systemInfoMissingCount;
        }
        if (record.nickname == QStringLiteral("obj_alpha")
            && record.usageType == IdsUsageType::IdsName) {
            ++objectNameMissingCount;
        }
        if (record.sectionName.compare(QStringLiteral("Room"), Qt::CaseInsensitive) == 0
            && record.usageType == IdsUsageType::IdsName) {
            ++roomNameMissingCount;
        }
        QVERIFY(record.nickname != QStringLiteral("zone_beta"));
        QVERIFY(record.nickname != QStringLiteral("buoy_gamma"));
    }
    QCOMPARE(systemInfoMissingCount, 1);
    QCOMPARE(objectNameMissingCount, 1);
    QCOMPARE(roomNameMissingCount, 1);

    bool foundMissingReferenceIssue = false;
    for (const auto &issue : dataset.issues) {
        if (issue.code == QStringLiteral("missing-referenced-id") && issue.globalId == 131072)
            foundMissingReferenceIssue = true;
    }
    QVERIFY(foundMissingReferenceIssue);
}

void TestIdsDataService::testAssignFieldValue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString iniPath = QDir(dir.path()).filePath(QStringLiteral("sample.ini"));
    {
        QFile ini(iniPath);
        QVERIFY(ini.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&ini);
        out << "[Object]\n"
            << "nickname = obj_alpha\n"
            << "ids_name = 0\n";
    }

    QString errorMessage;
    QVERIFY(IdsDataService::assignFieldValue(iniPath, 0, QStringLiteral("ids_name"), 196609, &errorMessage));
    QVERIFY(errorMessage.isEmpty());

    QFile ini(iniPath);
    QVERIFY(ini.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(ini.readAll());
    QVERIFY(text.contains(QStringLiteral("ids_name = 196609")));
}

void TestIdsDataService::testCreateEntriesPersistInFlatlasDll()
{
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("EXE")));
    QVERIFY(root.mkpath(QStringLiteral("DATA")));

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
    QVERIFY2(!templatePath.isEmpty(), "Expected a Freelancer name resource DLL template in the local workspace.");
    QVERIFY(QFile::copy(templatePath, root.filePath(QStringLiteral("EXE/NameResources.dll"))));

    {
        QFile freelancerIni(root.filePath(QStringLiteral("EXE/freelancer.ini")));
        QVERIFY(freelancerIni.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&freelancerIni);
        out << "[Resources]\n"
            << "DLL = NameResources.dll\n";
    }

    IdsDataset dataset = IdsDataService::loadFromGameRoot(dir.path());
    const int previewId1 = IdsDataService::nextAvailableGlobalId(dataset);
    QVERIFY(previewId1 > 0);

    int stringId = 0;
    QString errorMessage;
    QVERIFY2(IdsDataService::writeStringEntry(dataset,
                                              QString(),
                                              previewId1,
                                              QStringLiteral("Fresh FLAtlas string"),
                                              &stringId,
                                              &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(stringId, previewId1);

    dataset = IdsDataService::loadFromGameRoot(dir.path());
    const int previewId2 = IdsDataService::nextAvailableGlobalId(dataset);
    QVERIFY(previewId2 > previewId1);

    int htmlId = 0;
    QVERIFY2(IdsDataService::writeInfocardEntry(dataset,
                                                QString(),
                                                previewId2,
                                                QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-16\"?><RDL><TEXT>Fresh infocard</TEXT></RDL>"),
                                                &htmlId,
                                                &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(htmlId, previewId2);

    const IdsDataset reloaded = IdsDataService::loadFromGameRoot(dir.path());
    QVERIFY(reloaded.resourceDlls.contains(ResourceDllWriter::preferredFlatlasDllName()));
    QCOMPARE(IdsDataService::defaultCreationDllName(reloaded), ResourceDllWriter::preferredFlatlasDllName());
    const int previewId3 = IdsDataService::nextAvailableGlobalId(reloaded);
    QVERIFY(previewId3 > previewId2);

    bool foundString = false;
    bool foundHtml = false;
    for (const auto &entry : reloaded.entries) {
        if (entry.globalId == stringId) {
            foundString = true;
            QCOMPARE(entry.dllName, ResourceDllWriter::preferredFlatlasDllName());
            QCOMPARE(entry.stringValue, QStringLiteral("Fresh FLAtlas string"));
            QVERIFY(entry.editable);
        }
        if (entry.globalId == htmlId) {
            foundHtml = true;
            QCOMPARE(entry.dllName, ResourceDllWriter::preferredFlatlasDllName());
            QVERIFY(entry.htmlValue.contains(QStringLiteral("Fresh infocard")));
            QVERIFY(entry.editable);
        }
    }
    QVERIFY(foundString);
    QVERIFY(foundHtml);
#else
    QSKIP("Resource DLL writing is only supported on Windows.");
#endif
}

QTEST_GUILESS_MAIN(TestIdsDataService)
#include "test_IdsDataService.moc"