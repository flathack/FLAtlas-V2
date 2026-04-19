#include <QtTest>
#include "core/Theme.h"

using flatlas::core::Theme;

class TestTheme : public QObject {
    Q_OBJECT
private slots:
    void singletonIdentity()
    {
        auto &a = Theme::instance();
        auto &b = Theme::instance();
        QCOMPARE(&a, &b);
    }

    void availableThemesNotEmpty()
    {
        auto themes = Theme::instance().availableThemes();
        QVERIFY(themes.size() >= 4);
        QVERIFY(themes.contains(QStringLiteral("dark")));
        QVERIFY(themes.contains(QStringLiteral("light")));
        QVERIFY(themes.contains(QStringLiteral("founder")));
        QVERIFY(themes.contains(QStringLiteral("xp")));
    }

    void defaultThemeIsDark()
    {
        QCOMPARE(Theme::instance().currentTheme(), QStringLiteral("dark"));
    }

    void applyChangesCurrentTheme()
    {
        auto &theme = Theme::instance();
        QSignalSpy spy(&theme, &Theme::themeChanged);
        theme.apply(QStringLiteral("light"));
        QCOMPARE(theme.currentTheme(), QStringLiteral("light"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("light"));

        // Reset to dark
        theme.apply(QStringLiteral("dark"));
    }
};

QTEST_MAIN(TestTheme)
#include "test_Theme.moc"
