#include <QtTest>

#include "infrastructure/io/CmpLoader.h"

#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestLibertyBattleshipDeckPreviewBinding : public QObject {
    Q_OBJECT
private slots:
    void libertyBattleshipDeckBindingSnapshot()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/BASES/LIBERTY/li_battleship_deck.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local li_battleship_deck model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        QVERIFY2(decoded.isValid(), "Decoded li_battleship_deck model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(decoded.previewMaterialBindings.size(), 7);

        QStringList bindingSnapshot;
        for (const auto &binding : decoded.previewMaterialBindings) {
            bindingSnapshot.append(QStringLiteral("%1|%2|%3|%4")
                                      .arg(binding.modelName,
                                           binding.partName,
                                           binding.textureValue,
                                           binding.matchHint));
        }

        const QStringList expectedBindingSnapshot = {
            QStringLiteral("Deck021205120443.3db|Part_Deck|li_deck_floor.tga|token-match"),
            QStringLiteral("Door-01021205120443.3db|Part_Door-01|l_sh_01.tga|first-texture-fallback"),
            QStringLiteral("Door-02021205120443.3db|Part_Door-02|l_sh_01.tga|first-texture-fallback"),
            QStringLiteral("Gear01021205120443.3db|Part_Gear01|l_sh_01.tga|first-texture-fallback"),
            QStringLiteral("Gear04021205120443.3db|Part_Gear04|l_sh_01.tga|first-texture-fallback"),
            QStringLiteral("Root_deck021205120443.3db|Root|li_deck_floor.tga|token-match"),
            QStringLiteral("cap021205120443.3db|Part_cap|l_sh_01.tga|first-texture-fallback"),
        };

        QStringList sortedBindingSnapshot = bindingSnapshot;
        QStringList sortedExpectedBindingSnapshot = expectedBindingSnapshot;
        sortedBindingSnapshot.sort();
        sortedExpectedBindingSnapshot.sort();

        QCOMPARE(sortedBindingSnapshot, sortedExpectedBindingSnapshot);
    }
};

QTEST_GUILESS_MAIN(TestLibertyBattleshipDeckPreviewBinding)
#include "test_LibertyBattleshipDeckPreviewBinding.moc"
