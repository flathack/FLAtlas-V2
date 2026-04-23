#include <QtTest/QtTest>

#include "editors/system/ExclusionZoneUtils.h"

using namespace flatlas::editors;

class TestExclusionZoneUtils : public QObject
{
    Q_OBJECT

private slots:
    void patchFieldIniExclusionSection_writesShellOptions()
    {
        const QString original =
            "[Properties]\n"
            "flag = nebula\n"
            "\n"
            "[Fog]\n"
            "fog_enabled = 1\n";

        ExclusionShellSettings settings;
        settings.enabled = true;
        settings.fogFar = 8000;
        settings.zoneShell = QStringLiteral("solar\\nebula\\generic_exclusion.3db");
        settings.shellScalar = 1.0;
        settings.maxAlpha = 0.5;
        settings.exclusionTint = QStringLiteral("40, 120, 120");

        const auto patched = ExclusionZoneUtils::patchFieldIniExclusionSection(
            original,
            QStringLiteral("Zone_Li05_shipyard_exclusion"),
            settings);

        QVERIFY(patched.second);
        QVERIFY(patched.first.contains(QStringLiteral("[Exclusion Zones]")));
        QVERIFY(patched.first.contains(QStringLiteral("exclusion = Zone_Li05_shipyard_exclusion")));
        QVERIFY(patched.first.contains(QStringLiteral("fog_far = 8000")));
        QVERIFY(patched.first.contains(QStringLiteral("zone_shell = solar\\nebula\\generic_exclusion.3db")));
        QVERIFY(patched.first.contains(QStringLiteral("shell_scalar = 1")));
        QVERIFY(patched.first.contains(QStringLiteral("max_alpha = 0.5")));
        QVERIFY(patched.first.contains(QStringLiteral("exclusion_tint = 40, 120, 120")));
    }

    void patchFieldIniRemoveExclusion_removesFollowingOptions()
    {
        const QString original =
            "[Exclusion Zones]\n"
            "exclusion = Zone_Li05_shipyard_exclusion\n"
            "fog_far = 8000\n"
            "zone_shell = solar\\nebula\\generic_exclusion.3db\n"
            "shell_scalar = 1\n"
            "max_alpha = 0.5\n"
            "exclusion_tint = 40, 120, 120\n"
            "exclusion = Zone_Li05_other_exclusion\n"
            "exclude_billboards = 1\n";

        const auto patched = ExclusionZoneUtils::patchFieldIniRemoveExclusion(
            original,
            QStringLiteral("Zone_Li05_shipyard_exclusion"));

        QVERIFY(patched.second);
        QVERIFY(!patched.first.contains(QStringLiteral("Zone_Li05_shipyard_exclusion")));
        QVERIFY(!patched.first.contains(QStringLiteral("fog_far = 8000")));
        QVERIFY(!patched.first.contains(QStringLiteral("zone_shell = solar\\nebula\\generic_exclusion.3db")));
        QVERIFY(!patched.first.contains(QStringLiteral("shell_scalar = 1")));
        QVERIFY(!patched.first.contains(QStringLiteral("max_alpha = 0.5")));
        QVERIFY(!patched.first.contains(QStringLiteral("exclusion_tint = 40, 120, 120")));
        QVERIFY(patched.first.contains(QStringLiteral("exclusion = Zone_Li05_other_exclusion")));
        QVERIFY(patched.first.contains(QStringLiteral("exclude_billboards = 1")));
    }
};

QTEST_GUILESS_MAIN(TestExclusionZoneUtils)
#include "test_ExclusionZoneUtils.moc"
