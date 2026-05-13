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
        asteroid.zonePosX = 1200;
        asteroid.zonePosY = 50;
        asteroid.zonePosZ = -3400;
        asteroid.zoneRotateY = -40;
        asteroid.zoneShape = QStringLiteral("BOX");
        asteroid.zoneSizeX = 16000;
        asteroid.zoneSizeY = 6000;
        asteroid.zoneSizeZ = 22000;

        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(asteroid).startsWith(QStringLiteral("[Asteroids]\n")));
        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(asteroid).contains(QStringLiteral("pos = 1200, 50, -3400\n")));
        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(asteroid).contains(QStringLiteral("rotate = 0, -40, 0\n")));
        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(asteroid).contains(QStringLiteral("shape = BOX\n")));
        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(asteroid).contains(QStringLiteral("size = 16000, 6000, 22000\n")));
        QVERIFY(FieldTemplateGenerator::generateSystemLinkPreview(nebula).startsWith(QStringLiteral("[Nebula]\n")));
    }

    void parsesAsteroidTemplateForEditing()
    {
        const QString ini = QStringLiteral(
            "[TexturePanels]\n"
            "file = solar\\asteroids\\mine_shapes.ini\n"
            "\n"
            "[Field]\n"
            "cube_size = 275\n"
            "fill_dist = 2000\n"
            "diffuse_color = 214, 216, 255\n"
            "ambient_color = 96, 101, 128\n"
            "empty_cube_frequency = 0.25\n"
            "\n"
            "[properties]\n"
            "flag = MINE_DANGER_OBJECTS\n"
            "\n"
            "[Cube]\n"
            "xaxis_rotation = 8, 40, 90, 158\n"
            "yaxis_rotation = 5, 45, 100, 135\n"
            "zaxis_rotation = 355, 45, 78, 145\n"
            "asteroid = mine_spike1, -0.90, 0.00, 0.05, 110, 0, 10, mine\n"
            "\n"
            "[AsteroidBillboards]\n"
            "count = 120\n"
            "shape = spike_mine_tri\n"
            "\n"
            "[DynamicAsteroids]\n"
            "count = 0\n");

        const FieldTemplate field = FieldTemplateGenerator::parseFieldIni(QStringLiteral("loaded_mine.ini"), ini);

        QCOMPARE(field.kind, FieldTemplateKind::Mine);
        QCOMPARE(field.fileName, QStringLiteral("loaded_mine.ini"));
        QCOMPARE(field.texturePanelsFile, QStringLiteral("solar\\asteroids\\mine_shapes.ini"));
        QCOMPARE(field.cubeSize, 275);
        QCOMPARE(field.fillDistance, 2000);
        QCOMPARE(field.billboardCount, 120);
        QCOMPARE(field.dynamicCount, 0);
        QCOMPARE(field.cubeXAxisRotations, QVector<int>({8, 40, 90, 158}));
        QCOMPARE(field.cubeYAxisRotations, QVector<int>({5, 45, 100, 135}));
        QCOMPARE(field.cubeZAxisRotations, QVector<int>({355, 45, 78, 145}));
        QCOMPARE(field.placedObjects.size(), 1);
        QCOMPARE(field.placedObjects.first().assetNickname, QStringLiteral("mine_spike1"));
        QVERIFY(field.placedObjects.first().mineRole);

        const QString generated = FieldTemplateGenerator::generateFieldIni(field);
        QVERIFY(generated.contains(QStringLiteral("xaxis_rotation = 8, 40, 90, 158\n")));
        QVERIFY(generated.contains(QStringLiteral("yaxis_rotation = 5, 45, 100, 135\n")));
        QVERIFY(generated.contains(QStringLiteral("zaxis_rotation = 355, 45, 78, 145\n")));
        QCOMPARE(generated, ini);
    }

    void parsesNebulaTemplateForEditing()
    {
        const QString ini = QStringLiteral(
            "[TexturePanels]\n"
            "file = solar\\nebula\\crow_shapes.ini\n"
            "\n"
            "[Fog]\n"
            "distance = 1800\n"
            "color = 24, 56, 92\n"
            "\n"
            "[Exterior]\n"
            "fill_shape = nebula_circle2\n"
            "color = 53, 105, 157\n"
            "\n"
            "[NebulaLight]\n"
            "ambient = 41, 77, 104\n"
            "\n"
            "[Clouds]\n"
            "puff_count = 64\n"
            "puff_shape = crow_cloud1\n"
            "puff_shape = crow_cloud2\n"
            "\n"
            "[BackgroundLightning]\n"
            "duration = 0.50\n"
            "gap = 2.25\n");

        const FieldTemplate field = FieldTemplateGenerator::parseFieldIni(QStringLiteral("loaded_nebula.ini"), ini);

        QCOMPARE(field.kind, FieldTemplateKind::Nebula);
        QCOMPARE(field.fileName, QStringLiteral("loaded_nebula.ini"));
        QCOMPARE(field.texturePanelsFile, QStringLiteral("solar\\nebula\\crow_shapes.ini"));
        QCOMPARE(field.fogDistance, 1800);
        QCOMPARE(field.puffCount, 64);
        QCOMPARE(field.cubeShapeFallbacks, QStringList({QStringLiteral("crow_cloud1"), QStringLiteral("crow_cloud2")}));
        QCOMPARE(field.lightningGap, 2.25);
    }
};

QTEST_MAIN(FieldTemplateGeneratorTest)
#include "test_FieldTemplateGenerator.moc"
