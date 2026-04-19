#pragma once
// infrastructure/io/TextureLoader.h – DDS/TGA/TXM-Texturen
// TODO Phase 8
#include <QImage>
#include <QString>
namespace flatlas::infrastructure {
class TextureLoader {
public:
    static QImage loadDDS(const QString &filePath);
    static QImage loadTGA(const QString &filePath);
    static QImage loadTXM(const QString &filePath);
};
} // namespace flatlas::infrastructure
