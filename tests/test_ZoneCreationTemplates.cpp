#include <QTest>

#include "editors/system/SystemCreationDialogs.h"

using namespace flatlas::editors;

class TestZoneCreationTemplates : public QObject
{
    Q_OBJECT

private slots:
    void proposalsContainExpectedDefaults()
    {
        const QVector<ZoneCreationTemplateProposal> proposals =
            createSimpleZoneTemplateProposals(QStringLiteral("Li01"),
                                              QStringLiteral("Zone_LI01_zone_001"),
                                              {});

        QCOMPARE(proposals.size(), 4);
        QCOMPARE(proposals[0].templateId, ZoneCreationTemplate::Custom);
        QCOMPARE(proposals[0].nickname, QStringLiteral("Zone_LI01_zone_001"));
        QCOMPARE(proposals[0].shape, QStringLiteral("SPHERE"));
        QVERIFY(proposals[0].size.isEmpty());
        QCOMPARE(proposals[0].sort, 99);
        QVERIFY(proposals[0].rawEntries.isEmpty());

        QCOMPARE(proposals[1].templateId, ZoneCreationTemplate::DestroyVignette);
        QCOMPARE(proposals[1].nickname, QStringLiteral("Zone_Li01_destroy_vignette_01"));
        QCOMPARE(proposals[1].shape, QStringLiteral("SPHERE"));
        QCOMPARE(proposals[1].size, QStringLiteral("10000"));
        QCOMPARE(proposals[1].sort, 99);
        QCOMPARE(proposals[1].rawEntries.size(), 2);
        QCOMPARE(proposals[1].rawEntries[0].first, QStringLiteral("mission_type"));
        QCOMPARE(proposals[1].rawEntries[0].second, QStringLiteral("unlawful, lawful"));
        QCOMPARE(proposals[1].rawEntries[1].first, QStringLiteral("vignette_type"));
        QCOMPARE(proposals[1].rawEntries[1].second, QStringLiteral("open"));

        QCOMPARE(proposals[2].templateId, ZoneCreationTemplate::VignetteExclusion);
        QCOMPARE(proposals[2].nickname, QStringLiteral("Zone_Li01_vignette_exclusion_01"));
        QCOMPARE(proposals[2].size, QStringLiteral("10000"));
        QVERIFY(proposals[2].rawEntries.isEmpty());

        QCOMPARE(proposals[3].templateId, ZoneCreationTemplate::PopAmbient);
        QCOMPARE(proposals[3].nickname, QStringLiteral("Zone_Li01_pop_ambient_01"));
        QCOMPARE(proposals[3].shape, QStringLiteral("SPHERE"));
        QVERIFY(proposals[3].size.isEmpty());
        QVERIFY(proposals[3].rawEntries.isEmpty());
    }

    void templateNicknamesFollowExistingLocalNumbering()
    {
        const QStringList existing{
            QStringLiteral("zone_li01_destroy_vignette_1"),
            QStringLiteral("Zone_Li01_destroy_vignette_2"),
            QStringLiteral("Zone_Li01_pop_ambient_009"),
            QStringLiteral("Zone_Li01_vignette_exclusion_04"),
        };

        const QVector<ZoneCreationTemplateProposal> proposals =
            createSimpleZoneTemplateProposals(QStringLiteral("Li01"),
                                              QStringLiteral("Zone_LI01_zone_001"),
                                              existing);

        QCOMPARE(proposals[1].nickname, QStringLiteral("Zone_Li01_destroy_vignette_3"));
        QCOMPARE(proposals[2].nickname, QStringLiteral("Zone_Li01_vignette_exclusion_05"));
        QCOMPARE(proposals[3].nickname, QStringLiteral("Zone_Li01_pop_ambient_010"));
    }
};

QTEST_APPLESS_MAIN(TestZoneCreationTemplates)
#include "test_ZoneCreationTemplates.moc"
