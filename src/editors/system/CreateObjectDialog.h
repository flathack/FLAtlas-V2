#pragma once
// editors/system/CreateObjectDialog.h - Dialog zum Anlegen eines neuen Solar-Objekts.

#include <QDialog>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;

namespace flatlas::rendering { class ModelViewport3D; }
namespace flatlas::domain { class SystemDocument; }

namespace flatlas::editors {

/// Ergebniswerte des Dialogs. Die UI-Anzeigeformate werden hier bewusst
/// nicht wieder reingefaltet - was zurueckkommt ist genau das, was in
/// die system.ini geschrieben werden darf.
struct CreateObjectResult {
    QString nickname;
    QString ingameName;       ///< Optional - wird als ids_name-Platzhalter in den Kommentar geschrieben.
    QString archetype;
    QString loadout;          ///< Leer = nicht setzen.
    QString reputationNickname; ///< Reiner Faction-Nickname, kein Anzeigeformat.
};

class CreateObjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateObjectDialog(flatlas::domain::SystemDocument *document,
                                QWidget *parent = nullptr);

    CreateObjectResult result() const { return m_result; }

protected:
    void accept() override;

private:
    struct ArchetypeEntry {
        QString nickname;
        QString displayName;     ///< Ingame-Name via IDS, kann leer sein.
        QString modelPath;       ///< Absoluter Pfad zum .cmp/.3db, kann leer sein.
        QString defaultLoadout;  ///< solararch.ini loadout-Key, kann leer sein.
    };
    struct FactionEntry {
        QString nickname;
        QString displayText;     ///< "nickname - Ingame" oder "nickname" als Fallback.
    };

    void loadDataSources();
    void populateArchetypeCombo();
    void populateLoadoutCombo();
    void populateFactionCombo();
    void suggestNickname();
    void schedulePreviewRefresh();
    void refreshPreview();
    void applyDefaultLoadoutForCurrentArchetype();
    const ArchetypeEntry *resolveCurrentArchetype() const;
    static void configureSearchCompleter(QComboBox *combo);

    flatlas::domain::SystemDocument *m_document = nullptr;
    QString m_systemPrefix;     ///< Ausgangsprefix fuer die Nickname-Vorschlaege.

    QVector<ArchetypeEntry> m_archetypes;
    QStringList m_loadouts;
    QVector<FactionEntry> m_factions;

    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_ingameNameEdit = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QComboBox *m_factionCombo = nullptr;
    QLabel *m_archetypeStatus = nullptr;
    QLabel *m_loadoutHintLabel = nullptr;
    flatlas::rendering::ModelViewport3D *m_preview = nullptr;
    QLabel *m_previewFallback = nullptr;
    int m_previewLoadGeneration = 0;
    /// Tracks loadouts the user did not touch, so default-loadout suggestions
    /// may safely override them when the archetype changes.
    bool m_loadoutTouchedByUser = false;

    CreateObjectResult m_result;
};

} // namespace flatlas::editors
