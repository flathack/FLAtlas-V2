#include "BiniConverter.h"
#include "BiniDecoder.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace flatlas::infrastructure {

bool BiniConverter::convertFile(const QString &inputPath, const QString &outputPath)
{
    QFile inFile(inputPath);
    if (!inFile.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = inFile.readAll();
    inFile.close();

    if (!BiniDecoder::isBini(data))
        return false;

    QString text = BiniDecoder::decode(data);
    if (text.isEmpty())
        return false;

    // Ensure output directory exists
    QFileInfo outInfo(outputPath);
    QDir().mkpath(outInfo.absolutePath());

    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    outFile.write(text.toUtf8());
    outFile.close();
    return true;
}

int BiniConverter::convertDirectory(const QString &inputDir, const QString &outputDir)
{
    int count = 0;
    QDir inDir(inputDir);
    if (!inDir.exists())
        return 0;

    QDirIterator it(inputDir, {QStringLiteral("*.ini")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString relPath = inDir.relativeFilePath(it.filePath());
        QString outPath = outputDir + QLatin1Char('/') + relPath;

        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        QByteArray data = file.readAll();
        file.close();

        if (!BiniDecoder::isBini(data))
            continue;

        if (convertFile(it.filePath(), outPath))
            ++count;
    }
    return count;
}

int BiniConverter::countBiniFiles(const QString &directory)
{
    int count = 0;
    QDirIterator it(directory, {QStringLiteral("*.ini")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (BiniDecoder::isBiniFile(it.filePath()))
            ++count;
    }
    return count;
}

} // namespace flatlas::infrastructure
