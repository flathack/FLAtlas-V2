#include <QtTest>

#include "infrastructure/io/CmpLoader.h"

#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestBarbicanStationDeckPreviewBinding : public QObject {
    Q_OBJECT
private slots:
    void barbicanPartAwareBindingSnapshot()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/BASES/BRETONIA/br_barbican_station_deck.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local br_barbican_station_deck model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        QVERIFY2(decoded.isValid(), "Decoded br_barbican_station_deck model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(decoded.vmeshRefs.size(), 5);
        QCOMPARE(decoded.previewMaterialBindings.size(), 5);

        QStringList bindingSnapshot;
        for (const auto &binding : decoded.previewMaterialBindings) {
            QVERIFY2(!binding.partName.isEmpty(), "Expected part-aware preview binding");
            bindingSnapshot.append(QStringLiteral("%1|%2|%3|%4|%5")
                                      .arg(binding.modelName,
                                           binding.partName,
                                           binding.sourceNames.join(QStringLiteral(",")),
                                           binding.textureValue,
                                           binding.matchHint));
        }

        const QStringList expectedBindingSnapshot = {
            QStringLiteral("Door01021211170613.3db|Part_Door01|data.bases.bretonia.br_barbican_station_deck.lod0-112.vms,Door01021211170613.3db|iB_D_15.tga|first-texture-fallback"),
            QStringLiteral("Door02021211170613.3db|Part_Door02|data.bases.bretonia.br_barbican_station_deck.lod0-112.vms,Door02021211170613.3db|iB_D_15.tga|first-texture-fallback"),
            QStringLiteral("Door03021211170613.3db|Part_Door03|data.bases.bretonia.br_barbican_station_deck.lod0-112.vms,Door03021211170613.3db|iB_D_15.tga|first-texture-fallback"),
            QStringLiteral("Propeller 01021211170613.3db|Part_Propeller 01|data.bases.bretonia.br_barbican_station_deck.lod0-112.vms,Propeller 01021211170613.3db|iB_D_15.tga|first-texture-fallback"),
            QStringLiteral("Root_deck021211170613.3db|Root|data.bases.bretonia.br_barbican_station_deck.lod0-212.vms,Root_deck021211170613.3db|iB_D_15.tga|first-texture-fallback"),
        };

        QStringList sortedBindingSnapshot = bindingSnapshot;
        QStringList sortedExpectedBindingSnapshot = expectedBindingSnapshot;
        sortedBindingSnapshot.sort();
        sortedExpectedBindingSnapshot.sort();

        QCOMPARE(sortedBindingSnapshot, sortedExpectedBindingSnapshot);
    }
};

QTEST_GUILESS_MAIN(TestBarbicanStationDeckPreviewBinding)
#include "test_BarbicanStationDeckPreviewBinding.moc"