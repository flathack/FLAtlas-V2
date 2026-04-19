#include "BiniDecoder.h"
#include <QFile>
#include <QtEndian>
#include <cstring>
#include <cmath>

namespace flatlas::infrastructure {

// CP1252 bytes 0x80–0x9F differ from Latin1; map to correct Unicode codepoints.
static const char16_t cp1252Extra[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178  // 98-9F
};

bool BiniDecoder::isBini(const QByteArray &data)
{
    return data.size() >= 12 && data.startsWith("BINI");
}

bool BiniDecoder::isBiniFile(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QByteArray header = f.read(4);
    return header == "BINI";
}

QString BiniDecoder::getCStr(const QByteArray &strTable, int offset)
{
    if (offset < 0 || offset >= strTable.size())
        return QString();

    int end = strTable.indexOf('\0', offset);
    if (end < 0)
        end = strTable.size();

    int len = end - offset;
    QString result;
    result.reserve(len);
    const auto *raw = reinterpret_cast<const unsigned char *>(strTable.constData() + offset);
    for (int j = 0; j < len; ++j) {
        unsigned char ch = raw[j];
        if (ch >= 0x80 && ch <= 0x9F)
            result.append(QChar(cp1252Extra[ch - 0x80]));
        else
            result.append(QChar(ch));
    }
    return result;
}

QString BiniDecoder::fmtFloat(float v)
{
    // Match Python "%.7g"
    QString txt = QString::asprintf("%.7g", static_cast<double>(v));
    if (!txt.contains(QLatin1Char('.')) && !txt.contains(QLatin1Char('e')) && !txt.contains(QLatin1Char('E')))
        txt += QStringLiteral(".0");
    return txt;
}

QString BiniDecoder::decode(const QByteArray &data)
{
    if (!isBini(data))
        return {};

    const auto *raw = reinterpret_cast<const unsigned char *>(data.constData());
    auto readU16 = [&](int off) -> quint16 {
        quint16 v;
        std::memcpy(&v, raw + off, 2);
        return qFromLittleEndian(v);
    };
    auto readU32 = [&](int off) -> quint32 {
        quint32 v;
        std::memcpy(&v, raw + off, 4);
        return qFromLittleEndian(v);
    };
    auto readI32 = [&](int off) -> qint32 {
        qint32 v;
        std::memcpy(&v, raw + off, 4);
        return qFromLittleEndian(v);
    };
    auto readF32 = [&](int off) -> float {
        quint32 bits;
        std::memcpy(&bits, raw + off, 4);
        bits = qFromLittleEndian(bits);
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    };

    const int stringsOff = static_cast<int>(readU32(8));
    if (stringsOff < 12 || stringsOff > data.size())
        return {};

    const QByteArray strTable = data.mid(stringsOff);

    QStringList lines;
    int i = 12;
    bool firstSection = true;

    while (i + 4 <= stringsOff) {
        quint16 secOff = readU16(i);
        quint16 entryCount = readU16(i + 2);
        i += 4;

        QString secName = getCStr(strTable, secOff);
        if (secName.isEmpty())
            secName = QStringLiteral("Section");

        if (!firstSection)
            lines.append(QString());
        firstSection = false;

        lines.append(QStringLiteral("[") + secName + QStringLiteral("]"));

        for (int e = 0; e < entryCount; ++e) {
            if (i + 3 > stringsOff)
                break;
            quint16 keyOff = readU16(i);
            int valueCount = static_cast<unsigned char>(data[i + 2]);
            i += 3;

            QString keyName = getCStr(strTable, keyOff);
            if (keyName.isEmpty())
                keyName = QStringLiteral("key");

            QStringList values;
            for (int v = 0; v < valueCount; ++v) {
                if (i + 5 > data.size())
                    break;
                int typ = static_cast<unsigned char>(data[i]);
                i += 1;

                if (typ == 1) {
                    values.append(QString::number(readI32(i)));
                } else if (typ == 2) {
                    values.append(fmtFloat(readF32(i)));
                } else if (typ == 3) {
                    quint32 sOff = readU32(i);
                    values.append(getCStr(strTable, static_cast<int>(sOff)));
                }
                i += 4;
            }
            lines.append(keyName + QStringLiteral(" = ") + values.join(QStringLiteral(", ")));
        }
    }

    return lines.join(QStringLiteral("\n")) + QStringLiteral("\n");
}

} // namespace flatlas::infrastructure
