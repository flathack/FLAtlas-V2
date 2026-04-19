#include <QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include "editors/universe/UniverseSerializer.h"
#include "domain/UniverseData.h"

using namespace flatlas::editors;
using namespace flatlas::domain;

class TestUniverseSerializer : public QObject {
    Q_OBJECT
private slots:
    void loadNonExistentFile()
    {
        auto data = UniverseSerializer::load(QStringLiteral("nonexistent.ini"));
        QVERIFY(!data);
    }

    void loadEmptyFile()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("");
        file.close();
        auto data = UniverseSerializer::load(file.fileName());
        QVERIFY(!data);
    }

    void loadSingleSystem()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[System]\n"
                   "nickname = Li01\n"
                   "file = universe\\systems\\Li01\\Li01.ini\n"
                   "pos = -33000, 0\n"
                   "ids_name = 196610\n"
                   "strid_name = 196611\n");
        file.close();

        auto data = UniverseSerializer::load(file.fileName());
        QVERIFY(data);
        QCOMPARE(data->systemCount(), 1);
        QCOMPARE(data->systems[0].nickname, QStringLiteral("Li01"));
        QCOMPARE(data->systems[0].filePath, QStringLiteral("universe\\systems\\Li01\\Li01.ini"));
        QCOMPARE(data->systems[0].idsName, 196610);
        QCOMPARE(data->systems[0].stridName, 196611);
    }

    void loadMultipleSystems()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[System]\n"
                   "nickname = Li01\n"
                   "file = Li01.ini\n"
                   "pos = -33000, 0\n"
                   "[System]\n"
                   "nickname = Li02\n"
                   "file = Li02.ini\n"
                   "pos = 5000, 12000\n"
                   "[System]\n"
                   "nickname = Br01\n"
                   "file = Br01.ini\n"
                   "pos = 20000, -15000\n");
        file.close();

        auto data = UniverseSerializer::load(file.fileName());
        QVERIFY(data);
        QCOMPARE(data->systemCount(), 3);
        QCOMPARE(data->systems[1].nickname, QStringLiteral("Li02"));
    }

    void findSystem()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[System]\n"
                   "nickname = Li01\n"
                   "file = Li01.ini\n"
                   "pos = -33000, 0\n"
                   "[System]\n"
                   "nickname = Br01\n"
                   "file = Br01.ini\n"
                   "pos = 20000, -15000\n");
        file.close();

        auto data = UniverseSerializer::load(file.fileName());
        QVERIFY(data);

        auto *found = data->findSystem(QStringLiteral("Br01"));
        QVERIFY(found);
        QCOMPARE(found->nickname, QStringLiteral("Br01"));

        auto *notFound = data->findSystem(QStringLiteral("XX99"));
        QVERIFY(!notFound);
    }

    void findSystemCaseInsensitive()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[System]\n"
                   "nickname = Li01\n"
                   "file = Li01.ini\n"
                   "pos = 0, 0\n");
        file.close();

        auto data = UniverseSerializer::load(file.fileName());
        QVERIFY(data);
        QVERIFY(data->findSystem(QStringLiteral("li01")));
        QVERIFY(data->findSystem(QStringLiteral("LI01")));
    }

    void saveAndReload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.path() + "/universe_test.ini";

        UniverseData data;
        SystemInfo s1;
        s1.nickname = "TestSys1";
        s1.filePath = "systems/TestSys1.ini";
        s1.position = QVector3D(-5000, 0, 12000);
        s1.idsName = 100;
        data.addSystem(s1);

        SystemInfo s2;
        s2.nickname = "TestSys2";
        s2.filePath = "systems/TestSys2.ini";
        s2.position = QVector3D(8000, 0, -3000);
        data.addSystem(s2);

        QVERIFY(UniverseSerializer::save(data, filePath));

        auto loaded = UniverseSerializer::load(filePath);
        QVERIFY(loaded);
        QCOMPARE(loaded->systemCount(), 2);
        QCOMPARE(loaded->systems[0].nickname, QStringLiteral("TestSys1"));
        QCOMPARE(loaded->systems[0].filePath, QStringLiteral("systems/TestSys1.ini"));
        QCOMPARE(loaded->systems[0].idsName, 100);
        QCOMPARE(loaded->systems[1].nickname, QStringLiteral("TestSys2"));
    }

    void addAndRemoveSystem()
    {
        UniverseData data;
        QCOMPARE(data.systemCount(), 0);

        SystemInfo sys;
        sys.nickname = "Test";
        data.addSystem(sys);
        QCOMPARE(data.systemCount(), 1);
        QVERIFY(data.findSystem("Test"));
    }

    void positionParsing()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[System]\n"
                   "nickname = Li01\n"
                   "file = Li01.ini\n"
                   "pos = -33000, 75000\n");
        file.close();

        auto data = UniverseSerializer::load(file.fileName());
        QVERIFY(data);
        // pos = x, z (2D position)
        QCOMPARE(data->systems[0].position.x(), -33000.0f);
        QCOMPARE(data->systems[0].position.z(), 75000.0f);
    }
};

QTEST_GUILESS_MAIN(TestUniverseSerializer)
#include "test_UniverseSerializer.moc"
