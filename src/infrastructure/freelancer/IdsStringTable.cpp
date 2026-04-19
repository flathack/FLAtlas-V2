// infrastructure/freelancer/IdsStringTable.cpp – IDS-String-Tabelle (Phase 12)

#include "IdsStringTable.h"
#include "infrastructure/io/DllResources.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace flatlas::infrastructure {

void IdsStringTable::loadFromDll(const QString &dllPath)
{
    auto strings = DllResources::loadStrings(dllPath);
    for (auto it = strings.constBegin(); it != strings.constEnd(); ++it)
        m_strings.insert(it.key(), it.value());
}

void IdsStringTable::loadFromFreelancerDir(const QString &exeDir)
{
    // Standard Freelancer string DLLs in EXE directory
    QStringList dllNames = {
        QStringLiteral("resources.dll"),
        QStringLiteral("infocards.dll"),
        QStringLiteral("misctext.dll"),
        QStringLiteral("nameresources.dll"),
        QStringLiteral("equipresources.dll"),
        QStringLiteral("offerbriberesources.dll"),
        QStringLiteral("misctextinfo2.dll"),
    };

    QDir dir(exeDir);
    QStringList paths;
    for (const auto &name : dllNames) {
        QString path = dir.filePath(name);
        if (QFile::exists(path))
            paths.append(path);
    }

    if (!paths.isEmpty()) {
        auto merged = DllResources::loadMultipleDlls(paths);
        for (auto it = merged.constBegin(); it != merged.constEnd(); ++it)
            m_strings.insert(it.key(), it.value());
    }
}

void IdsStringTable::merge(const QMap<int, QString> &extra)
{
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        m_strings.insert(it.key(), it.value());
}

QString IdsStringTable::getString(int idsNumber) const
{
    return m_strings.value(idsNumber);
}

bool IdsStringTable::hasString(int idsNumber) const
{
    return m_strings.contains(idsNumber);
}

int IdsStringTable::count() const
{
    return m_strings.size();
}

QVector<int> IdsStringTable::allIds() const
{
    QVector<int> ids;
    ids.reserve(m_strings.size());
    for (auto it = m_strings.constBegin(); it != m_strings.constEnd(); ++it)
        ids.append(it.key());
    return ids;
}

bool IdsStringTable::exportCsv(const QString &filePath, QChar separator) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "id" << separator << "string\n";

    for (auto it = m_strings.constBegin(); it != m_strings.constEnd(); ++it) {
        QString val = it.value();
        // Escape quotes and newlines for CSV
        val.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        if (val.contains(separator) || val.contains('\n') || val.contains('"'))
            val = QLatin1Char('"') + val + QLatin1Char('"');
        out << it.key() << separator << val << '\n';
    }

    return true;
}

int IdsStringTable::importCsv(const QString &filePath, QChar separator)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    // Skip header
    if (!in.atEnd()) in.readLine();

    int imported = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        int sepIdx = line.indexOf(separator);
        if (sepIdx < 0) continue;

        bool ok = false;
        int id = line.left(sepIdx).toInt(&ok);
        if (!ok) continue;

        QString val = line.mid(sepIdx + 1);
        // Remove surrounding quotes
        if (val.startsWith('"') && val.endsWith('"')) {
            val = val.mid(1, val.size() - 2);
            val.replace(QStringLiteral("\"\""), QStringLiteral("\""));
        }

        m_strings.insert(id, val);
        ++imported;
    }

    return imported;
}

} // namespace flatlas::infrastructure
