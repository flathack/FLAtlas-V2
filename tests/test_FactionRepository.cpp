#include "infrastructure/freelancer/FactionRepository.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

using namespace flatlas::infrastructure;

namespace {

void writeText(const QString &path, const QString &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << text;
}

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestFactionRepository : public QObject {
    Q_OBJECT

private slots:
    void loadAndSaveRoundtripKeepsFactionDataAndLockedGates()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString root = temp.path();

        writeText(QDir(root).filePath(QStringLiteral("DATA/initialworld.ini")),
                  QStringLiteral("[Group]\n"
                                 "nickname = li_lsf_grp\n"
                                 "ids_name = 196846\n"
                                 "ids_info = 66501\n"
                                 "ids_short_name = 196847\n"
                                 "rep = 0.91, rh_n_grp\n"
                                 "\n"
                                 "[locked_gates]\n"
                                 "locked_gate = li01_to_li02\n"));
        writeText(QDir(root).filePath(QStringLiteral("DATA/MISSIONS/empathy.ini")),
                  QStringLiteral("[RepChangeEffects]\n"
                                 "group = li_lsf_grp\n"
                                 "event = object_destruction, -0.05\n"
                                 "empathy_rate = rh_n_grp, 0.25\n"));
        writeText(QDir(root).filePath(QStringLiteral("DATA/MISSIONS/faction_prop.ini")),
                  QStringLiteral("[FactionProps]\n"
                                 "affiliation = li_lsf_grp\n"
                                 "legality = lawful\n"
                                 "nickname_plurality = singular\n"
                                 "npc_ship = li_fighter\n"
                                 "npc_ship = li_elite\n"
                                 "voice = pilot_f_mil_m01\n"
                                 "formation = fighters\n"));

        FactionRepository repository;
        QString error;
        auto result = repository.load(root);
        QVERIFY2(result.warnings.isEmpty(), qPrintable(result.warnings.join(QLatin1Char('\n'))));
        auto *faction = result.world.faction(QStringLiteral("LI_LSF_GRP"));
        QVERIFY(faction);
        QCOMPARE(faction->idsName, QStringLiteral("196846"));
        QCOMPARE(faction->reputations.size(), 1);
        QCOMPARE(faction->props.npcShips.size(), 2);
        QCOMPARE(faction->props.voices.size(), 1);
        QCOMPARE(result.world.lockedGateEntries.size(), 1);

        result.world.setReputation(QStringLiteral("li_lsf_grp"), QStringLiteral("rh_n_grp"), -0.5);
        QVERIFY2(repository.save(result.world, root, &error), qPrintable(error));

        const QString initialWorld = readText(QDir(root).filePath(QStringLiteral("DATA/initialworld.ini")));
        QVERIFY(initialWorld.contains(QStringLiteral("rep = -0.5, rh_n_grp")));
        QVERIFY(initialWorld.contains(QStringLiteral("[locked_gates]")));
        QVERIFY(initialWorld.contains(QStringLiteral("locked_gate = li01_to_li02")));

        const QString props = readText(QDir(root).filePath(QStringLiteral("DATA/MISSIONS/faction_prop.ini")));
        QVERIFY(props.contains(QStringLiteral("npc_ship = li_fighter")));
        QVERIFY(props.contains(QStringLiteral("npc_ship = li_elite")));
    }
};

QTEST_APPLESS_MAIN(TestFactionRepository)
#include "test_FactionRepository.moc"
