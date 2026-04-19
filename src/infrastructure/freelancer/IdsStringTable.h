#pragma once
// infrastructure/freelancer/IdsStringTable.h – IDS-String-Tabelle
// TODO Phase 12
#include <QString>
#include <QMap>
namespace flatlas::infrastructure {
class IdsStringTable {
public:
    void loadFromDll(const QString &dllPath);
    QString getString(int idsNumber) const;
    bool hasString(int idsNumber) const;
    int count() const;
private:
    QMap<int, QString> m_strings;
};
} // namespace flatlas::infrastructure
