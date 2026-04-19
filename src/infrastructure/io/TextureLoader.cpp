// infrastructure/io/TextureLoader.cpp – DDS/TGA/TXM-Texturen (Phase 8)

#include "TextureLoader.h"
#include <QFile>
#include <QDataStream>
#include <QIODevice>
#include <QFileInfo>
#include <cstring>

namespace flatlas::infrastructure {

// ─── DDS constants ────────────────────────────────────────

static constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "
static constexpr uint32_t DDS_HEADER_SIZE = 124;

// DDS pixel format flags
static constexpr uint32_t DDPF_FOURCC = 0x04;
static constexpr uint32_t DDPF_RGB    = 0x40;
static constexpr uint32_t DDPF_RGBA   = 0x41;

static uint32_t makeFourCC(char a, char b, char c, char d) {
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
           (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

// ─── DXT decompression ───────────────────────────────────

static inline QRgb colorFrom565(uint16_t c) {
    int r = ((c >> 11) & 0x1F) * 255 / 31;
    int g = ((c >> 5) & 0x3F) * 255 / 63;
    int b = (c & 0x1F) * 255 / 31;
    return qRgba(r, g, b, 255);
}

void TextureLoader::decodeDXT1Block(const uchar *block, QRgb *outColors)
{
    uint16_t c0 = block[0] | (block[1] << 8);
    uint16_t c1 = block[2] | (block[3] << 8);
    QRgb color0 = colorFrom565(c0);
    QRgb color1 = colorFrom565(c1);

    QRgb palette[4];
    palette[0] = color0;
    palette[1] = color1;

    if (c0 > c1) {
        palette[2] = qRgba((2 * qRed(color0) + qRed(color1)) / 3,
                            (2 * qGreen(color0) + qGreen(color1)) / 3,
                            (2 * qBlue(color0) + qBlue(color1)) / 3, 255);
        palette[3] = qRgba((qRed(color0) + 2 * qRed(color1)) / 3,
                            (qGreen(color0) + 2 * qGreen(color1)) / 3,
                            (qBlue(color0) + 2 * qBlue(color1)) / 3, 255);
    } else {
        palette[2] = qRgba((qRed(color0) + qRed(color1)) / 2,
                            (qGreen(color0) + qGreen(color1)) / 2,
                            (qBlue(color0) + qBlue(color1)) / 2, 255);
        palette[3] = qRgba(0, 0, 0, 0); // transparent black
    }

    uint32_t bits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
    for (int i = 0; i < 16; ++i) {
        outColors[i] = palette[bits & 3];
        bits >>= 2;
    }
}

void TextureLoader::decodeDXT5Alpha(const uchar *block, uchar *outAlpha)
{
    uchar a0 = block[0];
    uchar a1 = block[1];

    uchar alphaTable[8];
    alphaTable[0] = a0;
    alphaTable[1] = a1;

    if (a0 > a1) {
        for (int i = 1; i <= 6; ++i)
            alphaTable[1 + i] = static_cast<uchar>(((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i <= 4; ++i)
            alphaTable[1 + i] = static_cast<uchar>(((5 - i) * a0 + i * a1) / 5);
        alphaTable[6] = 0;
        alphaTable[7] = 255;
    }

    // Decode 48-bit index block (6 bytes, 3 bits per pixel)
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i)
        bits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);

    for (int i = 0; i < 16; ++i) {
        outAlpha[i] = alphaTable[bits & 7];
        bits >>= 3;
    }
}

QImage TextureLoader::decompressDXT1(const QByteArray &data, int width, int height)
{
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(0);
    const uchar *src = reinterpret_cast<const uchar *>(data.constData());
    int blockCountX = (width + 3) / 4;
    int blockCountY = (height + 3) / 4;

    for (int by = 0; by < blockCountY; ++by) {
        for (int bx = 0; bx < blockCountX; ++bx) {
            QRgb colors[16];
            decodeDXT1Block(src, colors);
            src += 8;

            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    if (x < width && y < height)
                        img.setPixel(x, y, colors[py * 4 + px]);
                }
            }
        }
    }
    return img;
}

QImage TextureLoader::decompressDXT3(const QByteArray &data, int width, int height)
{
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(0);
    const uchar *src = reinterpret_cast<const uchar *>(data.constData());
    int blockCountX = (width + 3) / 4;
    int blockCountY = (height + 3) / 4;

    for (int by = 0; by < blockCountY; ++by) {
        for (int bx = 0; bx < blockCountX; ++bx) {
            // DXT3: 8 bytes explicit alpha, then 8 bytes color
            const uchar *alphaBlock = src;
            src += 8;
            QRgb colors[16];
            decodeDXT1Block(src, colors);
            src += 8;

            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int idx = py * 4 + px;
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    if (x < width && y < height) {
                        // Extract 4-bit alpha
                        int byteIdx = idx / 2;
                        int alpha4 = (idx & 1) ? (alphaBlock[byteIdx] >> 4) : (alphaBlock[byteIdx] & 0xF);
                        int alpha = alpha4 * 17; // Scale 0-15 to 0-255
                        QRgb c = colors[idx];
                        img.setPixel(x, y, qRgba(qRed(c), qGreen(c), qBlue(c), alpha));
                    }
                }
            }
        }
    }
    return img;
}

QImage TextureLoader::decompressDXT5(const QByteArray &data, int width, int height)
{
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(0);
    const uchar *src = reinterpret_cast<const uchar *>(data.constData());
    int blockCountX = (width + 3) / 4;
    int blockCountY = (height + 3) / 4;

    for (int by = 0; by < blockCountY; ++by) {
        for (int bx = 0; bx < blockCountX; ++bx) {
            uchar alphas[16];
            decodeDXT5Alpha(src, alphas);
            src += 8;

            QRgb colors[16];
            decodeDXT1Block(src, colors);
            src += 8;

            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int idx = py * 4 + px;
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    if (x < width && y < height) {
                        QRgb c = colors[idx];
                        img.setPixel(x, y, qRgba(qRed(c), qGreen(c), qBlue(c), alphas[idx]));
                    }
                }
            }
        }
    }
    return img;
}

// ─── DDS loading ──────────────────────────────────────────

QImage TextureLoader::loadDDSFromData(const QByteArray &data)
{
    if (data.size() < 128)
        return {};

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);

    uint32_t magic;
    ds >> magic;
    if (magic != DDS_MAGIC)
        return {};

    uint32_t headerSize;
    ds >> headerSize;
    if (headerSize != DDS_HEADER_SIZE)
        return {};

    uint32_t flags;
    ds >> flags;

    uint32_t height, width;
    ds >> height >> width;

    uint32_t pitchOrLinearSize, depth, mipMapCount;
    ds >> pitchOrLinearSize >> depth >> mipMapCount;

    // Skip reserved (11 uint32s)
    ds.skipRawData(11 * 4);

    // Pixel format (32 bytes)
    uint32_t pfSize, pfFlags, pfFourCC;
    uint32_t pfRGBBitCount, pfRMask, pfGMask, pfBMask, pfAMask;
    ds >> pfSize >> pfFlags >> pfFourCC;
    ds >> pfRGBBitCount >> pfRMask >> pfGMask >> pfBMask >> pfAMask;

    // Skip caps (4 uint32s)
    ds.skipRawData(4 * 4);
    // Skip reserved2
    ds.skipRawData(4);

    int headerTotal = 4 + DDS_HEADER_SIZE; // magic + header
    QByteArray pixelData = data.mid(headerTotal);

    if (pfFlags & DDPF_FOURCC) {
        if (pfFourCC == makeFourCC('D', 'X', 'T', '1'))
            return decompressDXT1(pixelData, width, height);
        if (pfFourCC == makeFourCC('D', 'X', 'T', '3'))
            return decompressDXT3(pixelData, width, height);
        if (pfFourCC == makeFourCC('D', 'X', 'T', '5'))
            return decompressDXT5(pixelData, width, height);
    }

    // Uncompressed RGBA
    if ((pfFlags & DDPF_RGB) && pfRGBBitCount == 32) {
        QImage img(width, height, QImage::Format_ARGB32);
        const uchar *src = reinterpret_cast<const uchar *>(pixelData.constData());
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                int offset = (y * width + x) * 4;
                if (offset + 3 < pixelData.size()) {
                    uchar b = src[offset], g = src[offset + 1];
                    uchar r = src[offset + 2], a = src[offset + 3];
                    img.setPixel(x, y, qRgba(r, g, b, a));
                }
            }
        }
        return img;
    }

    // Uncompressed RGB (24-bit)
    if ((pfFlags & DDPF_RGB) && pfRGBBitCount == 24) {
        QImage img(width, height, QImage::Format_RGB32);
        const uchar *src = reinterpret_cast<const uchar *>(pixelData.constData());
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                int offset = (y * width + x) * 3;
                if (offset + 2 < pixelData.size()) {
                    uchar b = src[offset], g = src[offset + 1], r = src[offset + 2];
                    img.setPixel(x, y, qRgb(r, g, b));
                }
            }
        }
        return img;
    }

    return {};
}

QImage TextureLoader::loadDDS(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return loadDDSFromData(file.readAll());
}

// ─── TGA loading ──────────────────────────────────────────

QImage TextureLoader::loadTGAFromData(const QByteArray &data)
{
    if (data.size() < 18)
        return {};

    const uchar *src = reinterpret_cast<const uchar *>(data.constData());

    uchar idLength = src[0];
    // uchar colorMapType = src[1];
    uchar imageType = src[2];
    // Color map spec: bytes 3-7 (skip)
    // Image spec:
    uint16_t width = src[12] | (src[13] << 8);
    uint16_t height = src[14] | (src[15] << 8);
    uchar bpp = src[16];
    uchar descriptor = src[17];

    bool topToBottom = (descriptor & 0x20) != 0;
    int pixelOffset = 18 + idLength;

    if (imageType == 2) {
        // Uncompressed true-color
        QImage::Format fmt = (bpp == 32) ? QImage::Format_ARGB32 : QImage::Format_RGB32;
        QImage img(width, height, fmt);
        int bytesPerPixel = bpp / 8;

        for (int y = 0; y < height; ++y) {
            int destY = topToBottom ? y : (height - 1 - y);
            for (int x = 0; x < width; ++x) {
                int off = pixelOffset + (y * width + x) * bytesPerPixel;
                if (off + bytesPerPixel > data.size()) break;
                uchar b = src[off], g = src[off + 1], r = src[off + 2];
                uchar a = (bpp == 32) ? src[off + 3] : 255;
                img.setPixel(x, destY, qRgba(r, g, b, a));
            }
        }
        return img;
    }

    if (imageType == 10) {
        // RLE-compressed true-color
        QImage::Format fmt = (bpp == 32) ? QImage::Format_ARGB32 : QImage::Format_RGB32;
        QImage img(width, height, fmt);
        int bytesPerPixel = bpp / 8;
        int pos = pixelOffset;
        int pixel = 0;
        int totalPixels = width * height;

        while (pixel < totalPixels && pos < data.size()) {
            uchar header = src[pos++];
            int count = (header & 0x7F) + 1;

            if (header & 0x80) {
                // RLE packet
                if (pos + bytesPerPixel > data.size()) break;
                uchar b = src[pos], g = src[pos + 1], r = src[pos + 2];
                uchar a = (bpp == 32) ? src[pos + 3] : 255;
                pos += bytesPerPixel;
                QRgb color = qRgba(r, g, b, a);
                for (int i = 0; i < count && pixel < totalPixels; ++i, ++pixel) {
                    int x = pixel % width;
                    int y = pixel / width;
                    int destY = topToBottom ? y : (height - 1 - y);
                    img.setPixel(x, destY, color);
                }
            } else {
                // Raw packet
                for (int i = 0; i < count && pixel < totalPixels; ++i, ++pixel) {
                    if (pos + bytesPerPixel > data.size()) break;
                    uchar b = src[pos], g = src[pos + 1], r = src[pos + 2];
                    uchar a = (bpp == 32) ? src[pos + 3] : 255;
                    pos += bytesPerPixel;
                    int x = pixel % width;
                    int y = pixel / width;
                    int destY = topToBottom ? y : (height - 1 - y);
                    img.setPixel(x, destY, qRgba(r, g, b, a));
                }
            }
        }
        return img;
    }

    return {};
}

QImage TextureLoader::loadTGA(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return loadTGAFromData(file.readAll());
}

// ─── TXM loading ──────────────────────────────────────────

QImage TextureLoader::loadTXM(const QString &filePath)
{
    // TXM files in Freelancer are essentially DDS files embedded in UTF containers.
    // Try loading as DDS first (some TXM files are just renamed DDS).
    QImage img = loadDDS(filePath);
    if (!img.isNull())
        return img;

    // Fallback: try loading as TGA
    return loadTGA(filePath);
}

// ─── Auto-detect ──────────────────────────────────────────

QImage TextureLoader::load(const QString &filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "dds")
        return loadDDS(filePath);
    if (ext == "tga")
        return loadTGA(filePath);
    if (ext == "txm")
        return loadTXM(filePath);

    // Try DDS first, then TGA
    QImage img = loadDDS(filePath);
    if (!img.isNull())
        return img;
    return loadTGA(filePath);
}

} // namespace flatlas::infrastructure
