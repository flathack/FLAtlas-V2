#pragma once
// infrastructure/io/TextureLoader.h – DDS/TGA/TXM-Texturen (Phase 8)
// Loads Freelancer texture formats into QImage.

#include <QImage>
#include <QString>
#include <QByteArray>

namespace flatlas::infrastructure {

class TextureLoader {
public:
    /// Load a DDS texture file (supports DXT1, DXT3, DXT5, uncompressed RGBA).
    static QImage loadDDS(const QString &filePath);

    /// Load a DDS texture from raw bytes (e.g. from UTF node data).
    static QImage loadDDSFromData(const QByteArray &data);

    /// Load a TGA texture file (uncompressed and RLE-compressed).
    static QImage loadTGA(const QString &filePath);

    /// Load a TGA texture from raw bytes.
    static QImage loadTGAFromData(const QByteArray &data);

    /// Load a Freelancer TXM texture file (DDS variant with TXM header).
    static QImage loadTXM(const QString &filePath);

    /// Auto-detect format and load.
    static QImage load(const QString &filePath);

private:
    static QImage decompressDXT1(const QByteArray &data, int width, int height);
    static QImage decompressDXT3(const QByteArray &data, int width, int height);
    static QImage decompressDXT5(const QByteArray &data, int width, int height);
    static void decodeDXT1Block(const uchar *block, QRgb *outColors);
    static void decodeDXT5Alpha(const uchar *block, uchar *outAlpha);
};

} // namespace flatlas::infrastructure
