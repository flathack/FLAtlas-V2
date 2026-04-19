#include <QtTest>
#include "app/AppConfig.h"

class TestAppConfig : public QObject {
    Q_OBJECT
private slots:
    void singletonIdentity()
    {
        auto &a = AppConfig::instance();
        auto &b = AppConfig::instance();
        QCOMPARE(&a, &b);
    }
    void defaultTheme()
    {
        auto &cfg = AppConfig::instance();
        QVERIFY(!cfg.theme().isEmpty());
    }
};

QTEST_MAIN(TestAppConfig)
#include "test_AppConfig.moc"
