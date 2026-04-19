#include <QtTest>
#include "core/I18n.h"

using flatlas::core::I18n;

class TestI18n : public QObject {
    Q_OBJECT
private slots:
    void singletonIdentity()
    {
        auto &a = I18n::instance();
        auto &b = I18n::instance();
        QCOMPARE(&a, &b);
    }

    void defaultLanguageIsDe()
    {
        QCOMPARE(I18n::instance().currentLanguage(), QStringLiteral("de"));
    }

    void availableLanguages()
    {
        auto langs = I18n::availableLanguages();
        QVERIFY(langs.contains(QStringLiteral("de")));
        QVERIFY(langs.contains(QStringLiteral("en")));
    }

    void setLanguageEmitsSignal()
    {
        auto &i18n = I18n::instance();
        QSignalSpy spy(&i18n, &I18n::languageChanged);
        i18n.setLanguage(QStringLiteral("en"));
        QCOMPARE(i18n.currentLanguage(), QStringLiteral("en"));
        QCOMPARE(spy.count(), 1);

        // Reset
        i18n.setLanguage(QStringLiteral("de"));
    }

    void setLanguageSameNoSignal()
    {
        auto &i18n = I18n::instance();
        i18n.setLanguage(QStringLiteral("de"));
        QSignalSpy spy(&i18n, &I18n::languageChanged);
        i18n.setLanguage(QStringLiteral("de")); // same
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestI18n)
#include "test_I18n.moc"
