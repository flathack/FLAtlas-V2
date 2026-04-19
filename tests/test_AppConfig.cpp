#include <QtTest>
#include "app/AppConfig.h"

class TestAppConfig : public QObject {
    Q_OBJECT
private slots:
    void singletonIdentity()
    {
        auto &a = flatlas::app::AppConfig::instance();
        auto &b = flatlas::app::AppConfig::instance();
        QCOMPARE(&a, &b);
    }
    void defaultTheme()
    {
        auto &cfg = flatlas::app::AppConfig::instance();
        QVERIFY(!cfg.theme().isEmpty());
    }
};

QTEST_MAIN(TestAppConfig)
#include "test_AppConfig.moc"
