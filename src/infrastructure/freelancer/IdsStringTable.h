#pragma once
// infrastructure/freelancer/IdsStringTable.h – IDS-String-Tabelle (Phase 12)

#include <QString>
#include <QMap>
#include <QStringList>

namespace flatlas::infrastructure {

class IdsStringTable {
public:
    /// Load all strings from a single DLL.
    void loadFromDll(const QString &dllPath);

    /// Load from standard Freelancer DLLs (resources.dll, infocards.dll, etc.)
    void loadFromFreelancerDir(const QString &exeDir);

    /// Merge additional strings (e.g. from a mod).
    void merge(const QMap<int, QString> &extra);

    /// Get string by IDS number. Returns empty if not found.
    QString getString(int idsNumber) const;

    /// Check if an IDS number exists.
    bool hasString(int idsNumber) const;

    /// Total loaded strings.
    int count() const;

    /// Get all IDS numbers (sorted).
    QVector<int> allIds() const;

    /// Access the full map (for iteration).
    const QMap<int, QString> &strings() const { return m_strings; }

    /// Export all strings to CSV (id;string).
    bool exportCsv(const QString &filePath, QChar separator = QLatin1Char(';')) const;

    /// Import strings from CSV (id;string) — merges/overwrites.
    int importCsv(const QString &filePath, QChar separator = QLatin1Char(';'));

private:
    QMap<int, QString> m_strings;
};

} // namespace flatlas::infrastructure
