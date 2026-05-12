#include "tools/FieldTemplateGenerator.h"

#include <QtTest/QtTest>

using flatlas::tools::FieldAsset;
using flatlas::tools::FieldTemplate;
using flatlas::tools::FieldTemplateGenerator;
using flatlas::tools::FieldTemplateKind;

class FieldTemplateGeneratorTest : public QObject
{
    Q_OBJECT

private slots:
    void asteroidPresetGeneratesCubeEntries()
    {
        FieldTemplate field = FieldTemplateGenerator::preset(FieldTemplateKind::Asteroid);
        field.placedObjects = {
            {QStringLiteral("minedout_asteroid10"), 0.6, 0.2, -0.2, 35, 10, 20, false},
            {QStringLiteral("mine_spike1"), -0.9, 0.0, 0.05, 110, 0, 10, true},
        };

        const QString ini = FieldTemplateGenerator::generateFieldIni(field);
        QVERIFY(ini.contains(QStringLiteral("[TexturePanels]\n")));
        QVERIFY(ini.contains(QStringLiteral("[Field]\n")));
        QVERIFY(ini.contains(QStringLiteral("[Cube]\n")));
        QVERIFY(ini.contains(QStringLiteral("asteroid = minedout_asteroid10, 0.60, 0.20, -0.20, 35, 10, 20\n")));
        QVERIFY(ini.contains(QStringLiteral("asteroid = mine_spike1, -0.90, 0.00, 0.05, 110, 0, 10, mine\n")));
        QVERIFY(!ini.contains(QStringLiteral("\r\r\n")));
    }

    void nebulaPresetGeneratesCloudSections()
    {
        const FieldTemplate field = FieldTemplateGenerator::preset(FieldTemplateKind::Nebula);

        const QString ini = FieldTemplateGenerator::generateFieldIni(field);
        QVERIFY(ini.contains(QStringLiteral("[Fog]\n")));
        QVERIFY(ini.contains(QStringLiteral("[Exterior]\n")));
        QVERIFY(ini.contains(QStringLiteral("[Clouds]\n")));
        QVERIFY(ini.contains(QStringLiteral("flag = nebula\n")));
        QVERIFY(!ini.contains(QStringLiteral("Copied by FL Atlas")));
    }

    void autoDistributionIsRepeatable()
    {
        const QVector<FieldAsset> assets = {
            {QStringLiteral("minedout_asteroid10"), {}, QStringLiteral("test")},
            {QStringLiteral("mine_spike1"), {}, QStringLiteral("test")},
        };

        const auto first = FieldTemplateGenerator::autoDistribute(
            assets, FieldTemplateKind::Mine, 8, 1209, QStringLiteral("Belt"));
        const auto second = FieldTemplateGenerator::autoDistribute(
            assets, FieldTemplateKind::Mine, 8, 1209, QStringLiteral("Belt"));

        QCOMPARE(first.size(), 8);
        QCOMPARE(second.size(), 8);
        for (int i = 0; i < first.size(); ++i) {
            QCOMPARE(first.at(i).assetNickname, second.at(i).assetNickname);
            QCOMPARE(first.at(i).x, second.at(i).x);
            QCOMPARE(first.at(i).y, second.at(i).y);
            QCOMPARE(first.at(i).z, second.at(i).z);
            QVERIFY(std::abs(first.at(i).y) <= 0.18);
            QVERIFY(first.at(i).mineRole);
        }
    }

    void linkPreviewChoosesCorrectSection()
    {
        FieldTemplate asteroid = FieldTemplateGenerator::preset(FieldTemplateKind::Asteroid);
        FieldTemplate nebula = FieldTemplateGenerator::preset(FieldTemplateKind::Nebula);

        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(asteroid).startsWith(QStringLiteral("[Asteroids]\n")));
        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(nebula).startsWith(QStringLiteral("[Nebula]\n")));
    }
};

QTEST_MAIN(FieldTemplateGeneratorTest)
#include "test_FieldTemplateGenerator.moc"
