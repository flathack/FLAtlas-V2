#pragma once
// rendering/view3d/SelectionManager.h – 3D-Objekt-Selektion (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include <QObject>
#include <QString>
#include <QHash>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QObjectPicker>
#include <Qt3DRender/QPickEvent>
#include <Qt3DExtras/QPhongMaterial>

namespace flatlas::rendering {

/// Manages selection of 3D entities by nickname. Highlights selected entity.
class SelectionManager : public QObject {
    Q_OBJECT
public:
    explicit SelectionManager(QObject *parent = nullptr);

    /// Register an entity with a nickname for picking.
    void registerEntity(const QString &nickname, Qt3DCore::QEntity *entity,
                        Qt3DExtras::QPhongMaterial *material);

    /// Unregister all entities.
    void clear();

    /// Select by nickname (e.g. from 2D map sync).
    void select(const QString &nickname);

    /// Current selection.
    QString selectedNickname() const { return m_selectedNickname; }

signals:
    void objectSelected(const QString &nickname);

private:
    void onPicked(Qt3DRender::QPickEvent *event, const QString &nickname);
    void applyHighlight(const QString &nickname, bool highlighted);

    struct EntityInfo {
        Qt3DCore::QEntity *entity = nullptr;
        Qt3DExtras::QPhongMaterial *material = nullptr;
        QColor originalDiffuse;
    };

    QHash<QString, EntityInfo> m_entities;
    QString m_selectedNickname;
    QColor m_highlightColor{255, 255, 0};  // Yellow highlight
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
