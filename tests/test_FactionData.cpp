#include "domain/FactionData.h"

#include <QtTest/QtTest>

using namespace flatlas::domain;

class TestFactionData : public QObject {
    Q_OBJECT

private slots:
    void addFactionKeepsOriginalCaseAndBuildsRelations()
    {
        FactionWorld world;
        world.addFaction(QStringLiteral("li_lsf_grp"));
        auto *liberty = world.faction(QStringLiteral("li_lsf_grp"));
        QVERIFY(liberty);
        liberty->idsName = QStringLiteral("196846");
        liberty->props.legality = QStringLiteral("lawful");

        world.addFaction(QStringLiteral("rh_n_grp"));
        auto *rheinland = world.faction(QStringLiteral("rh_n_grp"));
        QVERIFY(rheinland);
        rheinland->idsName = QStringLiteral("196847");
        rheinland->props.legality = QStringLiteral("lawful");

        QVERIFY(world.contains(QStringLiteral("LI_LSF_GRP")));
        QCOMPARE(world.faction(QStringLiteral("li_lsf_grp"))->nickname, QStringLiteral("li_lsf_grp"));
        QCOMPARE(world.reputation(QStringLiteral("li_lsf_grp"), QStringLiteral("li_lsf_grp"), 9.0), 0.91);
        QCOMPARE(world.reputation(QStringLiteral("rh_n_grp"), QStringLiteral("rh_n_grp"), 9.0), 0.91);
        QCOMPARE(world.reputation(QStringLiteral("li_lsf_grp"), QStringLiteral("rh_n_grp"), 9.0), 0.0);
        QCOMPARE(world.reputation(QStringLiteral("rh_n_grp"), QStringLiteral("LI_LSF_GRP"), 9.0), 0.0);
        QCOMPARE(world.empathyRate(QStringLiteral("li_lsf_grp"), QStringLiteral("rh_n_grp"), 9.0), 0.0);
    }

    void reputationIsClamped()
    {
        FactionWorld world;
        world.addFaction(QStringLiteral("a_grp"));
        world.addFaction(QStringLiteral("b_grp"));

        world.setReputation(QStringLiteral("a_grp"), QStringLiteral("b_grp"), 2.5);
        QCOMPARE(world.reputation(QStringLiteral("a_grp"), QStringLiteral("b_grp"), 0.0), 1.0);

        world.setReputation(QStringLiteral("a_grp"), QStringLiteral("b_grp"), -2.5);
        QCOMPARE(world.reputation(QStringLiteral("a_grp"), QStringLiteral("b_grp"), 0.0), -1.0);
    }

    void validationFindsBadTargetsAndCaseCollisions()
    {
        FactionWorld world;
        auto faction = Faction{};
        faction.nickname = QStringLiteral("fc_x_grp");
        faction.idsName = QStringLiteral("123");
        faction.props.affiliation = faction.nickname;
        faction.props.legality = QStringLiteral("unlawful");
        faction.reputations.append({QStringLiteral("missing_grp"), 0.2});
        faction.empathyRates.append({QStringLiteral("missing_grp"), 1.5});
        world.upsertFaction(faction);

        auto duplicateCase = faction;
        duplicateCase.nickname = QStringLiteral("FC_X_GRP");
        world.upsertFaction(duplicateCase);

        const auto issues = world.validate();
        bool hasMissingRepTarget = false;
        bool hasMissingEmpathyTarget = false;
        bool hasCaseCollision = false;
        for (const auto &issue : issues) {
            hasMissingRepTarget = hasMissingRepTarget || issue.message.contains(QStringLiteral("does not exist"));
            hasMissingEmpathyTarget = hasMissingEmpathyTarget || issue.message.contains(QStringLiteral("does not exist"));
            hasCaseCollision = hasCaseCollision || issue.message.contains(QStringLiteral("Case-collision"));
        }

        QVERIFY(hasMissingRepTarget);
        QVERIFY(hasMissingEmpathyTarget);
        QVERIFY(hasCaseCollision);
    }

    void validationFindsReputationOrderMismatch()
    {
        FactionWorld world;
        world.initialWorldRepOrder = {
            QStringLiteral("a_grp"),
            QStringLiteral("b_grp"),
            QStringLiteral("c_grp"),
        };

        Faction faction;
        faction.nickname = QStringLiteral("a_grp");
        faction.idsName = QStringLiteral("123");
        faction.idsInfo = QStringLiteral("456");
        faction.props.affiliation = faction.nickname;
        faction.props.legality = QStringLiteral("lawful");
        faction.inInitialWorld = true;
        faction.reputations = {
            {QStringLiteral("a_grp"), 0.91},
            {QStringLiteral("c_grp"), 0.0},
            {QStringLiteral("b_grp"), 0.0},
        };
        world.upsertFaction(faction);

        Faction b = faction;
        b.nickname = QStringLiteral("b_grp");
        b.reputations = {{QStringLiteral("a_grp"), 0.0}, {QStringLiteral("b_grp"), 0.91}, {QStringLiteral("c_grp"), 0.0}};
        world.upsertFaction(b);

        Faction c = faction;
        c.nickname = QStringLiteral("c_grp");
        c.reputations = {{QStringLiteral("a_grp"), 0.0}, {QStringLiteral("b_grp"), 0.0}, {QStringLiteral("c_grp"), 0.91}};
        world.upsertFaction(c);

        const auto issues = world.validate();
        bool hasOrderIssue = false;
        for (const auto &issue : issues) {
            hasOrderIssue = hasOrderIssue
                || (issue.faction == QStringLiteral("a_grp")
                    && issue.message.contains(QStringLiteral("initialworld.ini order")));
        }
        QVERIFY(hasOrderIssue);
    }
};

QTEST_APPLESS_MAIN(TestFactionData)
#include "test_FactionData.moc"
