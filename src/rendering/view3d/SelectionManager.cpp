// rendering/view3d/SelectionManager.cpp – 3D-Objekt-Selektion (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include "SelectionManager.h"
#include <Qt3DRender/QObjectPicker>

namespace flatlas::rendering {

SelectionManager::SelectionManager(QObject *parent)
    : QObject(parent)
{
}

void SelectionManager::registerEntity(const QString &nickname, Qt3DCore::QEntity *entity,
                                       Qt3DExtras::QPhongMaterial *material)
{
    EntityInfo info;
    info.entity = entity;
    info.material = material;
    info.originalDiffuse = material->diffuse();
    m_entities.insert(nickname, info);

    // Add object picker
    auto *picker = new Qt3DRender::QObjectPicker(entity);
    picker->setHoverEnabled(false);
    entity->addComponent(picker);

    connect(picker, &Qt3DRender::QObjectPicker::clicked,
            this, [this, nickname](Qt3DRender::QPickEvent *event) {
        onPicked(event, nickname);
    });
}

void SelectionManager::clear()
{
    m_selectedNickname.clear();
    m_entities.clear();
}

void SelectionManager::select(const QString &nickname)
{
    if (m_selectedNickname == nickname)
        return;

    // Unhighlight previous
    if (!m_selectedNickname.isEmpty())
        applyHighlight(m_selectedNickname, false);

    m_selectedNickname = nickname;

    // Highlight new
    if (!m_selectedNickname.isEmpty())
        applyHighlight(m_selectedNickname, true);

    emit objectSelected(m_selectedNickname);
}

void SelectionManager::onPicked(Qt3DRender::QPickEvent * /*event*/, const QString &nickname)
{
    select(nickname);
}

void SelectionManager::applyHighlight(const QString &nickname, bool highlighted)
{
    auto it = m_entities.find(nickname);
    if (it == m_entities.end())
        return;

    if (highlighted)
        it->material->setDiffuse(m_highlightColor);
    else
        it->material->setDiffuse(it->originalDiffuse);
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
