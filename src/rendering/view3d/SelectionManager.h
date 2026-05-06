#pragma once
// rendering/view3d/SelectionManager.h – 3D-Objekt-Selektion (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include <QObject>
#include <QString>
#include <QHash>
#include <QVector>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QObjectPicker>
#include <Qt3DRender/QPickEvent>

namespace Qt3DRender { class QMaterial; }

namespace flatlas::rendering {

/// Manages selection of 3D entities by nickname. Highlights selected entity.
class SelectionManager : public QObject {
    Q_OBJECT
public:
    explicit SelectionManager(QObject *parent = nullptr);

    /// Register an entity with a nickname for picking.
    void registerEntity(const QString &nickname, Qt3DCore::QEntity *entity,
                        Qt3DRender::QMaterial *material);

    /// Unregister all entities.
    void clear();

    /// Select by nickname (e.g. from 2D map sync).
    void select(const QString &nickname);

    /// Current selection.
    QString selectedNickname() const { return m_selectedNickname; }
    QString hoveredNickname() const { return m_hoveredNickname; }
    void setPickingSuppressed(bool suppressed) { m_pickingSuppressed = suppressed; }

signals:
    void objectSelected(const QString &nickname);
    void objectHovered(const QString &nickname);

private:
    void onPicked(Qt3DRender::QPickEvent *event, const QString &nickname);
    void setHovered(const QString &nickname);
    void applyHighlight(const QString &nickname, bool highlighted);
    void applyHoverHighlight(const QString &nickname, bool highlighted);
    static QColor diffuseColor(Qt3DRender::QMaterial *material);
    static void setDiffuseColor(Qt3DRender::QMaterial *material, const QColor &color);

    struct EntityInfo {
        Qt3DCore::QEntity *entity = nullptr;
        Qt3DRender::QMaterial *material = nullptr;
        QColor originalDiffuse;
    };

    QHash<QString, QVector<EntityInfo>> m_entities;
    QString m_selectedNickname;
    QString m_hoveredNickname;
    QColor m_highlightColor{255, 255, 0};  // Yellow highlight
    QColor m_hoverColor{120, 220, 255};
    bool m_pickingSuppressed = false;
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
