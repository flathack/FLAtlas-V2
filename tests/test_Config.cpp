#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/Config.h"

using flatlas::core::Config;

class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void singletonIdentity()
    {
        auto &a = Config::instance();
        auto &b = Config::instance();
        QCOMPARE(&a, &b);
    }

    void setGetString()
    {
        auto &cfg = Config::instance();
        cfg.setString(QStringLiteral("test_key"), QStringLiteral("hello"));
        QCOMPARE(cfg.getString(QStringLiteral("test_key")), QStringLiteral("hello"));
    }

    void setGetInt()
    {
        auto &cfg = Config::instance();
        cfg.setInt(QStringLiteral("test_int"), 42);
        QCOMPARE(cfg.getInt(QStringLiteral("test_int")), 42);
    }

    void setGetBool()
    {
        auto &cfg = Config::instance();
        cfg.setBool(QStringLiteral("test_bool"), true);
        QCOMPARE(cfg.getBool(QStringLiteral("test_bool")), true);
    }

    void defaultValues()
    {
        auto &cfg = Config::instance();
        QCOMPARE(cfg.getString(QStringLiteral("nonexistent"), QStringLiteral("def")),
                 QStringLiteral("def"));
        QCOMPARE(cfg.getInt(QStringLiteral("nonexistent"), 99), 99);
        QCOMPARE(cfg.getBool(QStringLiteral("nonexistent"), true), true);
    }

    void saveAndLoad()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString path = tmpDir.path() + QStringLiteral("/test_config.json");

        auto &cfg = Config::instance();
        cfg.setString(QStringLiteral("saved_key"), QStringLiteral("saved_value"));
        QVERIFY(cfg.save(path));

        // Verify the file exists and is valid JSON
        QFile file(path);
        QVERIFY(file.exists());
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto doc = QJsonDocument::fromJson(file.readAll());
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object().value(QStringLiteral("saved_key")).toString(),
                 QStringLiteral("saved_value"));
    }

    void loadFromFile()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString path = tmpDir.path() + QStringLiteral("/load_test.json");

        // Write a config file
        QJsonObject obj;
        obj[QStringLiteral("loaded_key")] = QStringLiteral("loaded_value");
        obj[QStringLiteral("loaded_int")] = 123;
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(obj).toJson());
        file.close();

        auto &cfg = Config::instance();
        QVERIFY(cfg.load(path));
        QCOMPARE(cfg.getString(QStringLiteral("loaded_key")),
                 QStringLiteral("loaded_value"));
        QCOMPARE(cfg.getInt(QStringLiteral("loaded_int")), 123);
    }
};

QTEST_MAIN(TestConfig)
#include "test_Config.moc"
