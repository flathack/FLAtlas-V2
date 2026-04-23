#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace flatlas::domain { class SystemDocument; }

namespace flatlas::editors {

struct CreateFieldZoneResult {
    enum class Type {
        Asteroid,
        Nebula
    };

    Type type = Type::Asteroid;
    QString nickname;
    QString ingameName;
    QString referenceFileName;
    QString referenceAbsolutePath;
    QString music;
    int damage = 0;
    int propertyFlags = 0;
    int visit = 0;
    int sort = 99;
    bool hasInterference = false;
    double interference = 0.0;
    QString spacedust;
    int spacedustMaxParticles = 50;
    QString comment;
};

class CreateFieldZoneDialog : public QDialog {
    Q_OBJECT
public:
    struct ReferenceEntry {
        QString fileName;
        QString absolutePath;
    };

    struct NumericOption {
        QString label;
        int value = 0;
    };

    explicit CreateFieldZoneDialog(flatlas::domain::SystemDocument *document,
                                   QWidget *parent = nullptr);

    CreateFieldZoneResult result() const { return m_result; }

protected:
    void accept() override;

private:
    void loadDataSources();
    void populateReferenceCombo();
    void populateMusicCombo();
    void populatePropertyFlagsCombo();
    void populateVisitCombo();
    void populateSpaceDustCombo();
    void updateTypeDependentFields();
    void suggestNickname();
    QString suggestedNicknameForType(CreateFieldZoneResult::Type type) const;
    QString normalizedNameToken(const QString &value) const;
    static void configureSearchCompleter(QComboBox *combo);
    static int numericComboValue(const QComboBox *combo, bool *ok = nullptr);
    static QString canonicalOptionText(const QComboBox *combo);

    flatlas::domain::SystemDocument *m_document = nullptr;
    QString m_systemToken;
    QString m_lastAutoNickname;
    bool m_nameEditedByUser = false;

    QVector<ReferenceEntry> m_asteroidReferences;
    QVector<ReferenceEntry> m_nebulaReferences;
    QStringList m_zoneMusicOptions;
    QStringList m_spaceDustOptions;

    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_ingameNameEdit = nullptr;
    QComboBox *m_referenceCombo = nullptr;
    QComboBox *m_musicCombo = nullptr;
    QSpinBox *m_damageSpin = nullptr;
    QComboBox *m_propertyFlagsCombo = nullptr;
    QComboBox *m_visitCombo = nullptr;
    QSpinBox *m_sortSpin = nullptr;
    QCheckBox *m_interferenceCheck = nullptr;
    QDoubleSpinBox *m_interferenceSpin = nullptr;
    QComboBox *m_spaceDustCombo = nullptr;
    QSpinBox *m_spaceDustParticlesSpin = nullptr;
    QLineEdit *m_commentEdit = nullptr;

    CreateFieldZoneResult m_result;
};

} // namespace flatlas::editors
