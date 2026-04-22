#include <QtTest/QtTest>

#include "infrastructure/freelancer/IdsDataService.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace flatlas::infrastructure;

class TestIdsDataService : public QObject {
    Q_OBJECT

private slots:
    void testScanMissingAndReferences();
    void testAssignFieldValue();
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
        out << "[Object]\n"
            << "nickname = obj_alpha\n"
            << "archetype = station\n"
            << "ids_name = 0\n"
            << "ids_info = 131072\n\n"
            << "[Zone]\n"
            << "nickname = zone_beta\n"
            << "ids_name = 0\n"
            << "ids_info = 0\n";
    }

    const IdsDataset dataset = IdsDataService::loadFromGameRoot(dir.path());
    QCOMPARE(dataset.references.size(), 1);
    QCOMPARE(dataset.references.first().globalId, 131072);
    QCOMPARE(dataset.references.first().usageType, IdsUsageType::IdsInfo);

    QCOMPARE(dataset.missingAssignments.size(), 3);

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

QTEST_GUILESS_MAIN(TestIdsDataService)
#include "test_IdsDataService.moc"