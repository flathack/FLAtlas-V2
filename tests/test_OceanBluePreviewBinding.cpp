#include <QtTest>

#include "infrastructure/io/CmpLoader.h"

#include <QFileInfo>

using namespace flatlas::infrastructure;

class TestOceanBluePreviewBinding : public QObject {
    Q_OBJECT
private slots:
    void oceanBlueTextureBindingSnapshot()
    {
        const QString filePath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/BASES/GENERIC/ocean_blue.cmp");

        if (!QFileInfo::exists(filePath))
            QSKIP("Local ocean_blue model not present.");

        const DecodedModel decoded = CmpLoader::loadModel(filePath);
        QVERIFY2(decoded.isValid(), "Decoded ocean_blue model is invalid");
        QCOMPARE(decoded.format, NativeModelFormat::Cmp);
        QCOMPARE(decoded.warnings.size(), 0);
        QCOMPARE(decoded.vmeshRefs.size(), 2);
        QCOMPARE(decoded.materialReferences.size(), 4);
        QCOMPARE(decoded.previewMaterialBindings.size(), 2);

        int textureRefCount = 0;
        for (const auto &reference : decoded.materialReferences) {
            if (reference.kind == QStringLiteral("texture"))
                ++textureRefCount;
        }
        QCOMPARE(textureRefCount, 4);

        QStringList bindingSnapshot;
        for (const auto &binding : decoded.previewMaterialBindings) {
            bindingSnapshot.append(QStringLiteral("%1|%2|%3|%4")
                                      .arg(binding.modelName,
                                           binding.textureValue,
                                           binding.textureCandidates.join(QStringLiteral(",")),
                                           binding.matchHint));
        }

        const QStringList expectedBindingSnapshot = {
            QStringLiteral("ocean_def_layer1021022165834.3db|ocean_a_256.tga|ocean_a_256.tga,ocean_256.tga|first-texture-fallback"),
            QStringLiteral("ocean_layer2021022165834.3db|ocean_a_256.tga|ocean_a_256.tga,ocean_256.tga|first-texture-fallback"),
        };

        QStringList sortedBindingSnapshot = bindingSnapshot;
        QStringList sortedExpectedBindingSnapshot = expectedBindingSnapshot;
        sortedBindingSnapshot.sort();
        sortedExpectedBindingSnapshot.sort();

        QCOMPARE(sortedBindingSnapshot, sortedExpectedBindingSnapshot);
    }
};

QTEST_GUILESS_MAIN(TestOceanBluePreviewBinding)
#include "test_OceanBluePreviewBinding.moc"