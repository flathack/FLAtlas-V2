#pragma once
// rendering/preview/CharacterPreview.h – Charakter-Vorschau (Phase 16)

#include <QDialog>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DExtras { class Qt3DWindow; }
namespace Qt3DCore { class QEntity; }
namespace Qt3DRender { class QCamera; }
#endif

namespace flatlas::rendering {

class OrbitCamera;

/// Dialog showing a character model with selectable costume/head/body variants.
class CharacterPreview : public QDialog {
    Q_OBJECT
public:
    explicit CharacterPreview(QWidget *parent = nullptr);
    ~CharacterPreview() override;

    /// Set the base directory containing character models.
    void setCharacterDir(const QString &dir);

    /// Load a specific body/head/accessory combination.
    void loadCharacter(const QString &body, const QString &head = {},
                       const QString &accessory = {});

    QString currentBody() const { return m_currentBody; }
    QString currentHead() const { return m_currentHead; }
    QStringList availableBodies() const { return m_bodies; }
    QStringList availableHeads() const { return m_heads; }

protected:
#ifdef FLATLAS_HAS_QT3D
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
#endif

private:
    void setupUi();
    void setupScene();
    void scanCharacterParts();
    void rebuildModel();

    QString m_characterDir;
    QString m_currentBody;
    QString m_currentHead;
    QStringList m_bodies;
    QStringList m_heads;

    QComboBox *m_bodyCombo = nullptr;
    QComboBox *m_headCombo = nullptr;
    QLabel *m_statusLabel = nullptr;

#ifdef FLATLAS_HAS_QT3D
    Qt3DExtras::Qt3DWindow *m_3dWindow = nullptr;
    QWidget *m_container = nullptr;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_charRoot = nullptr;
    Qt3DRender::QCamera *m_camera = nullptr;
    OrbitCamera *m_orbitCamera = nullptr;
#endif
};

} // namespace flatlas::rendering
