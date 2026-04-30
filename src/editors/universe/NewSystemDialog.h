#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

#include "domain/UniverseData.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QSpinBox;

namespace flatlas::editors {

struct NewSystemFactionOption {
    QString nickname;
    QString displayText;
};

struct NewSystemDialogOptions {
    QStringList musicSpaceOptions;
    QStringList musicDangerOptions;
    QStringList musicBattleOptions;
    QStringList basicStarsOptions;
    QStringList complexStarsOptions;
    QStringList nebulaeOptions;
    QVector<NewSystemFactionOption> factionOptions;
    QVector<flatlas::domain::SectorDefinition> sectors;
    QString activeSectorKey = QStringLiteral("universe");
};

struct NewSystemRequest {
    QString systemName;
    QString systemPrefix;
    int systemSize = 100000;
    QString spaceColor = QStringLiteral("0, 0, 0");
    QString musicSpace = QStringLiteral("music_br_space");
    QString musicDanger = QStringLiteral("music_br_danger");
    QString musicBattle = QStringLiteral("music_br_battle");
    QString ambientColor = QStringLiteral("60, 20, 10");
    QString basicStars = QStringLiteral("solar\\starsphere\\starsphere_stars_basic.cmp");
    QString complexStars = QStringLiteral("solar\\starsphere\\starsphere_br01_stars.cmp");
    QString nebulae = QStringLiteral("solar\\starsphere\\starsphere_br01.cmp");
    QString lightSourceColor = QStringLiteral("253, 230, 180");
    QString localFactionNickname = QStringLiteral("li_n_grp");
    QString localFactionDisplay;
    QString sectorKey = QStringLiteral("universe");
};

class NewSystemDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewSystemDialog(const NewSystemDialogOptions &options, QWidget *parent = nullptr);

    NewSystemRequest request() const;

private:
    QComboBox *createEditableCombo(const QStringList &items, const QString &defaultValue) const;
    QWidget *createColorRow(QLabel *&valueLabel, const QString &initialValue,
                            const QString &dialogTitle);

    NewSystemDialogOptions m_options;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_prefixEdit = nullptr;
    QSpinBox *m_sizeSpin = nullptr;
    QLabel *m_spaceColorLabel = nullptr;
    QLabel *m_ambientColorLabel = nullptr;
    QLabel *m_lightColorLabel = nullptr;
    QComboBox *m_musicSpaceCombo = nullptr;
    QComboBox *m_musicDangerCombo = nullptr;
    QComboBox *m_musicBattleCombo = nullptr;
    QComboBox *m_basicStarsCombo = nullptr;
    QComboBox *m_complexStarsCombo = nullptr;
    QComboBox *m_nebulaeCombo = nullptr;
    QComboBox *m_localFactionCombo = nullptr;
    QComboBox *m_sectorCombo = nullptr;
};

} // namespace flatlas::editors
