#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "infrastructure/parser/IniAnalysisService.h"

using namespace flatlas::infrastructure;

class TestIniAnalysisService : public QObject {
    Q_OBJECT
private slots:
    void detectsSectionsDiagnosticsAndLineMapping();
    void searchesAcrossIniFiles();
};

void TestIniAnalysisService::detectsSectionsDiagnosticsAndLineMapping()
{
    const QString text = QStringLiteral(
        "orphan = value\n"
        "[System]\n"
        "nickname = Li01\n"
        "nickname = Li01_alt\n"
        "broken line\n"
        "[Base]\n"
        "nickname = Li01_01_Base\n");

    const IniAnalysisResult result = IniAnalysisService::analyzeText(text);
    QCOMPARE(result.sections.size(), 2);
    QCOMPARE(result.sections.at(0).name, QStringLiteral("System"));
    QCOMPARE(result.sections.at(1).name, QStringLiteral("Base"));
    QCOMPARE(result.sectionIndexForLine(3), 0);
    QCOMPARE(result.sectionIndexForLine(7), 1);

    QVERIFY(result.diagnostics.size() >= 3);

    bool foundOrphan = false;
    bool foundDuplicateKey = false;
    bool foundMissingEquals = false;
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == QStringLiteral("entry-outside-section"))
            foundOrphan = true;
        if (diagnostic.code == QStringLiteral("duplicate-key"))
            foundDuplicateKey = true;
        if (diagnostic.code == QStringLiteral("missing-equals"))
            foundMissingEquals = true;
    }

    QVERIFY(foundOrphan);
    QVERIFY(foundDuplicateKey);
    QVERIFY(foundMissingEquals);
}

void TestIniAnalysisService::searchesAcrossIniFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile systemFile(dir.filePath(QStringLiteral("universe.ini")));
    QVERIFY(systemFile.open(QIODevice::WriteOnly | QIODevice::Text));
    systemFile.write("[System]\nnickname = Li01\nfile = systems\\li01\\li01.ini\n");
    systemFile.close();

    QFile baseFile(dir.filePath(QStringLiteral("bases.ini")));
    QVERIFY(baseFile.open(QIODevice::WriteOnly | QIODevice::Text));
    baseFile.write("[Base]\nnickname = Li01_01_Base\n");
    baseFile.close();

    IniSearchOptions options;
    options.query = QStringLiteral("Li01");
    const QVector<IniSearchMatch> matches = IniAnalysisService::searchDirectory(dir.path(), options);

    QVERIFY(matches.size() >= 2);

    bool foundSystem = false;
    bool foundBase = false;
    for (const auto &match : matches) {
        if (match.sectionName == QStringLiteral("System"))
            foundSystem = true;
        if (match.sectionName == QStringLiteral("Base"))
            foundBase = true;
    }

    QVERIFY(foundSystem);
    QVERIFY(foundBase);
}

QTEST_MAIN(TestIniAnalysisService)
#include "test_IniAnalysisService.moc"