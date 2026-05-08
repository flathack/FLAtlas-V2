#pragma once

#include <QDialog>
#include <QHash>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

namespace flatlas::editors {

class ZonePopulationDialog : public QDialog {
    Q_OBJECT
public:
    explicit ZonePopulationDialog(const QString &zoneNickname,
                                  const QVector<QPair<QString, QString>> &entries,
                                  const QStringList &encounterParameters,
                                  const QStringList &allEncounters,
                                  const QStringList &factions,
                                  QWidget *parent = nullptr);

    QVector<QPair<QString, QString>> entries() const;
    QSet<QString> newEncounterParameters() const { return m_newEncounterParameters; }

private slots:
    void addDensityRestriction();
    void removeDensityRestriction();
    void addEncounter();
    void addFaction();
    void removeEncounterItem();
    void accept() override;

private:
    struct EncounterRow {
        QString name;
        QString level;
        QString chance;
        QVector<QPair<QString, QString>> factions;
    };

    QString inferProfile() const;
    QStringList popTypesForProfile(const QString &profile) const;
    QHash<QString, QString> defaultsForProfile(const QString &profile) const;
    QString profileSummaryText(const QString &profile) const;
    QVector<EncounterRow> encounterRows() const;
    QStringList densityRestrictions() const;
    QStringList validationErrors(QStringList *warnings) const;

    static QString factionNicknameFromDisplay(const QString &raw);
    static int toInt(const QString &value, int fallback = 0);
    static double toDouble(const QString &value, double fallback = 0.0);
    static QString formatFloat(const QString &value, const QString &fallback);

    QVector<QPair<QString, QString>> m_otherEntries;
    QString m_initialPopType;
    QStringList m_encounterParameters;
    QStringList m_allEncounters;
    QStringList m_factions;
    QSet<QString> m_newEncounterParameters;
    QString m_profile;
    QHash<QString, QString> m_defaults;

    QSpinBox *m_toughnessSpin = nullptr;
    QSpinBox *m_densitySpin = nullptr;
    QSpinBox *m_repopSpin = nullptr;
    QSpinBox *m_battleSpin = nullptr;
    QComboBox *m_popTypeCombo = nullptr;
    QSpinBox *m_reliefSpin = nullptr;
    QListWidget *m_densityList = nullptr;
    QTreeWidget *m_encounterTree = nullptr;
};

} // namespace flatlas::editors
