#pragma once
// infrastructure/io/CsvImporter.h – CSV-Import (IDS, News, Rumors)
// TODO Phase 12/18
#include <QString>
#include <QVector>
#include <QStringList>
namespace flatlas::infrastructure {
class CsvImporter {
public:
    static QVector<QStringList> parse(const QString &filePath, QChar separator = QLatin1Char(','));
};
} // namespace flatlas::infrastructure
