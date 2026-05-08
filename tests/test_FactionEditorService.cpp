#include "editors/faction/FactionEditorService.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

using namespace flatlas::editors;

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

void writeFactionFixture(const QString &root)
{
    writeText(QDir(root).filePath(QStringLiteral("DATA/initialworld.ini")),
              QStringLiteral("[Group]\n"
                             "nickname = li_lsf_grp\n"
                             "ids_name = 1\n"
                             "rep = 0.5, rh_n_grp\n"
                             "\n"
                             "[Group]\n"
                             "nickname = rh_n_grp\n"
                             "ids_name = 2\n"
                             "rep = 0.5, li_lsf_grp\n"));
    writeText(QDir(root).filePath(QStringLiteral("DATA/MISSIONS/empathy.ini")),
              QStringLiteral("[RepChangeEffects]\n"
                             "group = li_lsf_grp\n"
                             "empathy_rate = rh_n_grp, 0.25\n"
                             "\n"
                             "[RepChangeEffects]\n"
                             "group = rh_n_grp\n"
                             "empathy_rate = li_lsf_grp, 0.25\n"));
    writeText(QDir(root).filePath(QStringLiteral("DATA/MISSIONS/faction_prop.ini")),
              QStringLiteral("[FactionProps]\n"
                             "affiliation = li_lsf_grp\n"
                             "legality = lawful\n"
                             "\n"
                             "[FactionProps]\n"
                             "affiliation = rh_n_grp\n"
                             "legality = lawful\n"));
    writeText(QDir(root).filePath(QStringLiteral("DATA/SYSTEMS/li01.ini")),
              QStringLiteral("[Object]\n"
                             "nickname = test_patrol\n"
                             "faction = li_lsf_grp\n"));
}

} // namespace

class TestFactionEditorService : public QObject {
    Q_OBJECT

private slots:
    void unchangedEditorValuesDoNotSetDirty()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        writeFactionFixture(temp.path());

        FactionEditorService service;
        QString error;
        QVERIFY2(service.load(temp.path(), &error), qPrintable(error));
        QVERIFY(!service.isDirty());

        const auto *loaded = service.faction(QStringLiteral("li_lsf_grp"));
        QVERIFY(loaded);
        const flatlas::domain::FactionPropData props = loaded->props;

        service.setIds(QStringLiteral("li_lsf_grp"), loaded->idsName, loaded->idsInfo, loaded->idsShortName);
        service.setProperties(QStringLiteral("li_lsf_grp"),
                              props,
                              loaded->inInitialWorld,
                              loaded->inEmpathy,
                              loaded->inFactionProp);
        service.setReputation(QStringLiteral("li_lsf_grp"), QStringLiteral("rh_n_grp"), 0.5);
        service.setEmpathyRate(QStringLiteral("li_lsf_grp"), QStringLiteral("rh_n_grp"), 0.25);

        QVERIFY(!service.isDirty());

        service.setIds(QStringLiteral("li_lsf_grp"), QStringLiteral("42"), loaded->idsInfo, loaded->idsShortName);
        QVERIFY(service.isDirty());
    }

    void deactivateCanReplaceReferencesAndThenDelete()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        writeFactionFixture(temp.path());

        FactionEditorService service;
        QString error;
        QVERIFY2(service.load(temp.path(), &error), qPrintable(error));

        const auto beforeRefs = service.referencesForFaction(QStringLiteral("li_lsf_grp"));
        bool hasIncomingRep = false;
        bool hasExternal = false;
        for (const auto &ref : beforeRefs) {
            hasIncomingRep = hasIncomingRep || (ref.scope == QStringLiteral("Reputation") && ref.source == QStringLiteral("rh_n_grp"));
            hasExternal = hasExternal || ref.externalFileReference;
        }
        QVERIFY(hasIncomingRep);
        QVERIFY(hasExternal);

        QVERIFY(!service.deactivateFaction(QStringLiteral("li_lsf_grp"), QString(), &error));
        QVERIFY(error.contains(QStringLiteral("must be replaced"), Qt::CaseInsensitive));
        QVERIFY(readText(QDir(temp.path()).filePath(QStringLiteral("DATA/SYSTEMS/li01.ini")))
                    .contains(QStringLiteral("faction = li_lsf_grp")));

        QVERIFY2(service.deactivateFaction(QStringLiteral("li_lsf_grp"), QStringLiteral("rh_n_grp"), &error),
                 qPrintable(error));
        const auto *liberty = service.world().faction(QStringLiteral("li_lsf_grp"));
        QVERIFY(liberty);
        QVERIFY(!liberty->inInitialWorld);
        QVERIFY(!liberty->inEmpathy);
        QVERIFY(!liberty->inFactionProp);
        QCOMPARE(service.world().reputation(QStringLiteral("rh_n_grp"), QStringLiteral("li_lsf_grp"), 99.0), 99.0);
        QVERIFY(readText(QDir(temp.path()).filePath(QStringLiteral("DATA/SYSTEMS/li01.ini")))
                    .contains(QStringLiteral("faction = rh_n_grp")));

        QVERIFY2(service.deleteFaction(QStringLiteral("li_lsf_grp"), QString(), &error), qPrintable(error));
        QVERIFY(!service.world().contains(QStringLiteral("li_lsf_grp")));
    }
};

QTEST_APPLESS_MAIN(TestFactionEditorService)
#include "test_FactionEditorService.moc"
