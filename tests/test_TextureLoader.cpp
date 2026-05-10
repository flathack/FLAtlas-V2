#include <QtTest>
#include <QFileInfo>
#include <QStringList>
#include <functional>
#include "infrastructure/freelancer/FreelancerMaterialResolver.h"
#include "infrastructure/io/CmpLoader.h"
#include "infrastructure/io/TextureLoader.h"

using namespace flatlas::infrastructure;

class TestTextureLoader : public QObject {
    Q_OBJECT
private slots:
    void loadDDSNonExistent()
    {
        QImage img = TextureLoader::loadDDS(QStringLiteral("nonexistent.dds"));
        QVERIFY(img.isNull());
    }

    void loadTGANonExistent()
    {
        QImage img = TextureLoader::loadTGA(QStringLiteral("nonexistent.tga"));
        QVERIFY(img.isNull());
    }

    void loadAutoDetectNonExistent()
    {
        QImage img = TextureLoader::load(QStringLiteral("nonexistent.dds"));
        QVERIFY(img.isNull());
    }

    void loadDDSFromDataEmpty()
    {
        QImage img = TextureLoader::loadDDSFromData(QByteArray());
        QVERIFY(img.isNull());
    }

    void loadDDSFromDataTooShort()
    {
        QImage img = TextureLoader::loadDDSFromData(QByteArray(50, '\0'));
        QVERIFY(img.isNull());
    }

    void loadDDSFromDataBadMagic()
    {
        QByteArray data(256, '\0');
        data[0] = 'B'; data[1] = 'A'; data[2] = 'D'; data[3] = '!';
        QImage img = TextureLoader::loadDDSFromData(data);
        QVERIFY(img.isNull());
    }

    void loadTGAFromDataEmpty()
    {
        QImage img = TextureLoader::loadTGAFromData(QByteArray());
        QVERIFY(img.isNull());
    }

    void loadTGAFromDataTooShort()
    {
        QImage img = TextureLoader::loadTGAFromData(QByteArray(10, '\0'));
        QVERIFY(img.isNull());
    }

    void loadDDSFromDataSyntheticDXT1()
    {
        // Build a minimal 4x4 DXT1 DDS file
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::LittleEndian);

        // DDS magic
        ds << uint32_t(0x20534444);

        // Header (124 bytes)
        ds << uint32_t(124);      // header size
        ds << uint32_t(0x1007);   // flags (CAPS|HEIGHT|WIDTH|PIXELFORMAT)
        ds << uint32_t(4);        // height
        ds << uint32_t(4);        // width
        ds << uint32_t(8);        // pitchOrLinearSize (1 block DXT1 = 8 bytes)
        ds << uint32_t(0);        // depth
        ds << uint32_t(0);        // mipMapCount

        // Reserved (11 uint32s)
        for (int i = 0; i < 11; ++i) ds << uint32_t(0);

        // Pixel format (32 bytes)
        ds << uint32_t(32);       // pfSize
        ds << uint32_t(0x04);     // pfFlags = DDPF_FOURCC
        // FourCC "DXT1"
        ds << uint8_t('D') << uint8_t('X') << uint8_t('T') << uint8_t('1');
        ds << uint32_t(0);        // rgbBitCount
        ds << uint32_t(0);        // rMask
        ds << uint32_t(0);        // gMask
        ds << uint32_t(0);        // bMask
        ds << uint32_t(0);        // aMask

        // Caps (4 uint32s)
        ds << uint32_t(0x1000);   // caps1
        ds << uint32_t(0) << uint32_t(0) << uint32_t(0);

        // Reserved2
        ds << uint32_t(0);

        // DXT1 block data (8 bytes for 4x4): all red
        // Color0 = 0xF800 (R=31,G=0,B=0), Color1 = 0x0000 (black)
        ds << uint16_t(0xF800);   // color0
        ds << uint16_t(0x0000);   // color1
        // All pixels use color0 (index 0 = 00b for all 16 pixels)
        ds << uint32_t(0x00000000);

        QImage img = TextureLoader::loadDDSFromData(data);
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), 4);
        QCOMPARE(img.height(), 4);

        // All pixels should be red (255, 0, 0)
        QRgb pixel = img.pixel(0, 0);
        QCOMPARE(qRed(pixel), 255);
        QCOMPARE(qGreen(pixel), 0);
        QCOMPARE(qBlue(pixel), 0);
    }

    void loadTGAFromDataSyntheticUncompressed()
    {
        // Build a 2x2 uncompressed TGA (type 2, 24bpp)
        QByteArray data;
        data.resize(18 + 2 * 2 * 3); // header + 4 pixels * 3 bytes

        uchar *d = reinterpret_cast<uchar*>(data.data());
        d[0] = 0;    // idLength
        d[1] = 0;    // colorMapType
        d[2] = 2;    // imageType = uncompressed
        // Color map spec (5 bytes)
        d[3] = d[4] = d[5] = d[6] = d[7] = 0;
        // Image spec
        d[8] = d[9] = 0;     // xOrigin
        d[10] = d[11] = 0;   // yOrigin
        d[12] = 2; d[13] = 0; // width = 2
        d[14] = 2; d[15] = 0; // height = 2
        d[16] = 24;           // bpp
        d[17] = 0;            // descriptor (bottom to top)

        // Pixel data: BGR
        // Row 0 (bottom): red, green
        d[18] = 0; d[19] = 0; d[20] = 255;    // pixel (0,0) = red (BGR)
        d[21] = 0; d[22] = 255; d[23] = 0;    // pixel (1,0) = green
        // Row 1 (top): blue, white
        d[24] = 255; d[25] = 0; d[26] = 0;    // pixel (0,1) = blue
        d[27] = 255; d[28] = 255; d[29] = 255; // pixel (1,1) = white

        QImage img = TextureLoader::loadTGAFromData(data);
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), 2);
        QCOMPARE(img.height(), 2);

        // Bottom-to-top: row 0 in file = bottom of image (y=1 in Qt)
        QRgb topLeft = img.pixel(0, 0);     // row 1 in file = blue
        QCOMPARE(qRed(topLeft), 0);
        QCOMPARE(qGreen(topLeft), 0);
        QCOMPARE(qBlue(topLeft), 255);

        QRgb bottomLeft = img.pixel(0, 1);  // row 0 in file = red
        QCOMPARE(qRed(bottomLeft), 255);
        QCOMPARE(qGreen(bottomLeft), 0);
        QCOMPARE(qBlue(bottomLeft), 0);
    }

    void loadAutoDetectByExtension()
    {
        // Ensure .dds extension routes to DDS loader
        QImage img = TextureLoader::load(QStringLiteral("test.dds"));
        QVERIFY(img.isNull()); // File doesn't exist, but shouldn't crash

        img = TextureLoader::load(QStringLiteral("test.tga"));
        QVERIFY(img.isNull());

        img = TextureLoader::load(QStringLiteral("test.txm"));
        QVERIFY(img.isNull());
    }

    void loadPlanetTextureFromFreelancerHdMipsNode()
    {
        const QString dataRoot = QStringLiteral("C:/Users/steve/Github/FL-Installationen/Freelancer-HD/DATA");
        auto verifyPlanet = [](const QString &archetype,
                               const QString &txmPath,
                               const QString &matPath,
                               const QString &sphPath) {
            const QImage image = FreelancerMaterialResolver::loadBestPlanetTexture(
                archetype, QStringList{txmPath, matPath, sphPath});
            const auto segments =
                FreelancerMaterialResolver::loadPlanetSurfaceTextures(QStringList{txmPath, matPath, sphPath});
            QVERIFY(segments.hasSegmentedSurface());
            QVERIFY(!segments.side1.isNull());
            QVERIFY(!segments.side2.isNull());
            QVERIFY(!segments.cap.isNull());
            QVERIFY(!image.isNull());
            QVERIFY(image.width() > 0);
            QVERIFY(image.height() > 0);
            QVERIFY2(image.width() >= image.height() * 2 - 1,
                     qPrintable(QStringLiteral("%1 produced %2x%3")
                                    .arg(archetype)
                                    .arg(image.width())
                                    .arg(image.height())));
        };

        QString txmPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/planet_earthgrncld.txm");
        QString matPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/PLANET_EARTHGRNCLD/planet_earthgrncld.mat");
        QString sphPath = dataRoot
                          + QStringLiteral("/SOLAR/PLANETS/PLANET_EARTHGRNCLD/planet_earthgrncld_4000.sph");
        if (!QFileInfo::exists(txmPath) || !QFileInfo::exists(matPath) || !QFileInfo::exists(sphPath))
            QSKIP("Freelancer-HD reference planet files are not available on this machine.");
        verifyPlanet(QStringLiteral("planet_earthgrncld_4000"), txmPath, matPath, sphPath);

        txmPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/planet_rckbrn.txm");
        matPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/PLANET_RCKBRN/planet_rckbrn.mat");
        sphPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/PLANET_RCKBRN/planet_rckbrn_2000.sph");
        if (!QFileInfo::exists(txmPath) || !QFileInfo::exists(matPath) || !QFileInfo::exists(sphPath))
            QSKIP("Freelancer-HD Malta reference planet files are not available on this machine.");
        verifyPlanet(QStringLiteral("planet_rckbrn_2000"), txmPath, matPath, sphPath);

        txmPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/planet_icelndwat.txm");
        matPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/PLANET_ICELNDWAT/planet_icelndwat.mat");
        sphPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/PLANET_ICELNDWAT/planet_icelndwat_3000.sph");
        if (!QFileInfo::exists(txmPath) || !QFileInfo::exists(matPath) || !QFileInfo::exists(sphPath))
            QSKIP("Freelancer-HD Rh01_01 reference planet files are not available on this machine.");
        verifyPlanet(QStringLiteral("planet_icelndwat_3000"), txmPath, matPath, sphPath);

        txmPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/planet_desored.txm");
        matPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/PLANET_DESORED/planet_desored.mat");
        sphPath = dataRoot + QStringLiteral("/SOLAR/PLANETS/PLANET_DESORED/planet_desored_1500.sph");
        if (!QFileInfo::exists(txmPath) || !QFileInfo::exists(matPath) || !QFileInfo::exists(sphPath))
            QSKIP("Freelancer-HD Li01_02 reference planet files are not available on this machine.");
        verifyPlanet(QStringLiteral("planet_desored_1500"), txmPath, matPath, sphPath);
    }

    void loadStationTextureFromExternalMaterialLibrary()
    {
        const QString modelPath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/Freelancer-HD/DATA/SOLAR/DOCKABLE/station_large_a_lod.cmp");
        const QString matPath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/Freelancer-HD/DATA/SOLAR/solar_mat_dockable01.mat");
        if (!QFileInfo::exists(modelPath) || !QFileInfo::exists(matPath))
            QSKIP("Freelancer-HD station material fixture is not available on this machine.");

        const DecodedModel decoded = CmpLoader::loadModel(modelPath);
        QVERIFY(decoded.isValid());

        QImage resolvedTexture;
        std::function<void(const ModelNode &)> walk = [&](const ModelNode &node) {
            if (!resolvedTexture.isNull())
                return;
            for (const MeshData &mesh : node.meshes) {
                const QImage image = FreelancerMaterialResolver::loadTextureForMesh(modelPath, mesh);
                if (!image.isNull()) {
                    resolvedTexture = image;
                    return;
                }
            }
            for (const ModelNode &child : node.children)
                walk(child);
        };
        walk(decoded.rootNode);

        QVERIFY(!resolvedTexture.isNull());
        QVERIFY(resolvedTexture.width() > 0);
        QVERIFY(resolvedTexture.height() > 0);
    }

    void loadLibertyDreadnoughtTexturesFromExternalMaterialLibrary()
    {
        const QString modelPath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SHIPS/LIBERTY/li_dreadnought/li_dreadnought.cmp");
        const QString matPath = QStringLiteral(
            "C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SHIPS/LIBERTY/li_capships.mat");
        if (!QFileInfo::exists(modelPath) || !QFileInfo::exists(matPath))
            QSKIP("TESTMOD1 Liberty dreadnought material fixture is not available on this machine.");

        const DecodedModel decoded = CmpLoader::loadModel(modelPath);
        QVERIFY(decoded.isValid());

        int meshCount = 0;
        int texturedMeshCount = 0;
        QStringList unresolvedMaterials;
        std::function<void(const ModelNode &)> walk = [&](const ModelNode &node) {
            for (const MeshData &mesh : node.meshes) {
                ++meshCount;
                const QImage image = FreelancerMaterialResolver::loadTextureForMesh(modelPath, mesh);
                if (!image.isNull()) {
                    ++texturedMeshCount;
                } else if (mesh.materialId >= 0) {
                    unresolvedMaterials.append(QStringLiteral("%1:%2").arg(mesh.materialId).arg(mesh.materialName));
                } else if (!mesh.materialName.isEmpty()) {
                    unresolvedMaterials.append(mesh.materialName);
                }
            }
            for (const ModelNode &child : node.children)
                walk(child);
        };
        walk(decoded.rootNode);

        QVERIFY(meshCount > 0);
        QVERIFY2(texturedMeshCount == meshCount,
                 qPrintable(QStringLiteral("Only %1 of %2 Liberty dreadnought meshes resolved textures; unresolved: %3")
                                .arg(texturedMeshCount)
                                .arg(meshCount)
                                .arg(unresolvedMaterials.join(QStringLiteral(", ")))));
    }

    void loadLi01_07StationTexturesFromExternalMaterialLibrary()
    {
        const QStringList modelPaths{
            QStringLiteral("C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/space_factory01.cmp"),
            QStringLiteral("C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/shipyard_a.3db"),
        };
        const QString matPath =
            QStringLiteral("C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/Solar_mat_tink.mat");
        for (const QString &modelPath : modelPaths) {
            if (!QFileInfo::exists(modelPath) || !QFileInfo::exists(matPath))
                QSKIP("TESTMOD1 Li01_07 station material fixture is not available on this machine.");

            const DecodedModel decoded = CmpLoader::loadModel(modelPath);
            QVERIFY2(decoded.isValid(), qPrintable(QStringLiteral("Invalid model: %1").arg(modelPath)));

            int meshCount = 0;
            int texturedMeshCount = 0;
            QStringList unresolvedMaterials;
            std::function<void(const ModelNode &)> walk = [&](const ModelNode &node) {
                for (const MeshData &mesh : node.meshes) {
                    ++meshCount;
                    const QImage image = FreelancerMaterialResolver::loadTextureForMesh(QString(), mesh);
                    if (!image.isNull()) {
                        ++texturedMeshCount;
                    } else if (!mesh.materialName.isEmpty() || !mesh.materialValue.isEmpty()) {
                        unresolvedMaterials.append(QStringLiteral("%1:%2:%3")
                                                       .arg(mesh.materialId)
                                                       .arg(mesh.materialName, mesh.materialValue));
                    } else {
                        unresolvedMaterials.append(QStringLiteral("%1:<empty>").arg(mesh.materialId));
                    }
                }
                for (const ModelNode &child : node.children)
                    walk(child);
            };
            walk(decoded.rootNode);

            QVERIFY2(meshCount > 0, qPrintable(QStringLiteral("No meshes decoded for %1").arg(modelPath)));
            QVERIFY2(texturedMeshCount == meshCount,
                     qPrintable(QStringLiteral("Only %1 of %2 Li01_07 station meshes resolved textures for %3; unresolved=%4")
                                    .arg(texturedMeshCount)
                                    .arg(meshCount)
                                    .arg(modelPath)
                                    .arg(unresolvedMaterials.join(QStringLiteral(", ")))));
        }
    }
};

QTEST_GUILESS_MAIN(TestTextureLoader)
#include "test_TextureLoader.moc"
