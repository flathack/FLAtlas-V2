// infrastructure/io/CsvImporter.cpp – CSV-Import (Phase 12)

#include "CsvImporter.h"

#include <QFile>
#include <QTextStream>

namespace flatlas::infrastructure {

QVector<QStringList> CsvImporter::parse(const QString &filePath, QChar separator)
{
    QVector<QStringList> result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields;
        QString field;
        bool inQuotes = false;

        for (int i = 0; i < line.size(); ++i) {
            QChar ch = line[i];
            if (inQuotes) {
                if (ch == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        field += '"';
                        ++i; // skip escaped quote
                    } else {
                        inQuotes = false;
                    }
                } else {
                    field += ch;
                }
            } else {
                if (ch == '"') {
                    inQuotes = true;
                } else if (ch == separator) {
                    fields.append(field);
                    field.clear();
                } else {
                    field += ch;
                }
            }
        }
        fields.append(field);
        result.append(fields);
    }

    return result;
}

} // namespace flatlas::infrastructure
