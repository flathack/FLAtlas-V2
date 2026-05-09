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

    void defaultLanguageIsEn()
    {
        QCOMPARE(I18n::instance().currentLanguage(), QStringLiteral("en"));
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
        i18n.setLanguage(QStringLiteral("de"));
        QCOMPARE(i18n.currentLanguage(), QStringLiteral("de"));
        QCOMPARE(spy.count(), 1);

        // Reset
        i18n.setLanguage(QStringLiteral("en"));
    }

    void germanJsonTranslation()
    {
        auto &i18n = I18n::instance();
        i18n.setLanguage(QStringLiteral("de"));
        QCOMPARE(QCoreApplication::translate("test", "&File"), QStringLiteral("&Datei"));
        QCOMPARE(QCoreApplication::translate("test", "Opening system: %1"), QStringLiteral("Öffne System: %1"));
        i18n.setLanguage(QStringLiteral("en"));
    }

    void englishFallsBackToSourceText()
    {
        auto &i18n = I18n::instance();
        i18n.setLanguage(QStringLiteral("en"));
        QCOMPARE(QCoreApplication::translate("test", "&File"), QStringLiteral("&File"));
    }

    void setLanguageSameNoSignal()
    {
        auto &i18n = I18n::instance();
        i18n.setLanguage(QStringLiteral("en"));
        QSignalSpy spy(&i18n, &I18n::languageChanged);
        i18n.setLanguage(QStringLiteral("en")); // same
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestI18n)
#include "test_I18n.moc"
