#include "IniParser.h"
#include "BiniDecoder.h"

#include <QFile>
#include <QStringDecoder>

namespace flatlas::infrastructure {

QStringList IniSection::values(const QString &key) const
{
    QStringList result;
    for (const auto &e : entries)
        if (e.first.compare(key, Qt::CaseInsensitive) == 0)
            result.append(e.second);
    return result;
}

QString IniSection::value(const QString &key, const QString &defaultValue) const
{
    for (const auto &e : entries)
        if (e.first.compare(key, Qt::CaseInsensitive) == 0)
            return e.second;
    return defaultValue;
}

IniDocument IniParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray raw = file.readAll();
    file.close();

    if (BiniDecoder::isBini(raw))
        return parseText(BiniDecoder::decode(raw));

    // Try UTF-8 first, fallback to Latin1/CP1252
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8(raw);
    if (!utf8.hasError())
        return parseText(text);

    // Fallback: Latin1 (superset of CP1252 printable range)
    return parseText(QString::fromLatin1(raw));
}

IniDocument IniParser::parseText(const QString &text)
{
    IniDocument doc;
    QString curName;
    QVector<IniEntry> curEntries;
    bool inSection = false;

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        QString line = raw.trimmed();

        if (line.isEmpty() || line.startsWith(QLatin1Char(';'))
            || line.startsWith(QLatin1String("//")))
            continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            if (inSection) {
                IniSection sec;
                sec.name = curName;
                sec.entries = curEntries;
                doc.append(sec);
            }
            curName = line.mid(1, line.length() - 2).trimmed();
            curEntries.clear();
            inSection = true;
            continue;
        }

        const int eqPos = line.indexOf(QLatin1Char('='));
        if (eqPos > 0 && inSection) {
            // Strip inline ";" comments
            int semPos = line.indexOf(QLatin1Char(';'), eqPos + 1);
            if (semPos > 0)
                line = line.left(semPos).trimmed();

            QString key = line.left(eqPos).trimmed();
            QString val = line.mid(eqPos + 1).trimmed();
            curEntries.append({key, val});
        }
    }

    if (inSection) {
        IniSection sec;
        sec.name = curName;
        sec.entries = curEntries;
        doc.append(sec);
    }

    return doc;
}

QString IniParser::serialize(const IniDocument &doc)
{
    QString out;
    for (int i = 0; i < doc.size(); ++i) {
        const IniSection &sec = doc[i];
        if (i > 0)
            out += QLatin1Char('\n');
        out += QLatin1Char('[') + sec.name + QLatin1String("]\n");
        for (const auto &e : sec.entries)
            out += e.first + QLatin1String(" = ") + e.second + QLatin1Char('\n');
    }
    return out;
}

} // namespace flatlas::infrastructure
