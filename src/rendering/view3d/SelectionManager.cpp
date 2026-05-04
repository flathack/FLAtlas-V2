// rendering/view3d/SelectionManager.cpp – 3D-Objekt-Selektion (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include "SelectionManager.h"

#include <Qt3DExtras/QPhongAlphaMaterial>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DRender/QObjectPicker>

namespace flatlas::rendering {

SelectionManager::SelectionManager(QObject *parent)
    : QObject(parent)
{
}

void SelectionManager::registerEntity(const QString &nickname, Qt3DCore::QEntity *entity,
                                       Qt3DRender::QMaterial *material)
{
    EntityInfo info;
    info.entity = entity;
    info.material = material;
    info.originalDiffuse = diffuseColor(material);
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
        setDiffuseColor(it->material, m_highlightColor);
    else
        setDiffuseColor(it->material, it->originalDiffuse);
}

QColor SelectionManager::diffuseColor(Qt3DRender::QMaterial *material)
{
    if (const auto *phong = qobject_cast<Qt3DExtras::QPhongMaterial *>(material))
        return phong->diffuse();
    if (const auto *alpha = qobject_cast<Qt3DExtras::QPhongAlphaMaterial *>(material))
        return alpha->diffuse();
    return QColor();
}

void SelectionManager::setDiffuseColor(Qt3DRender::QMaterial *material, const QColor &color)
{
    if (auto *phong = qobject_cast<Qt3DExtras::QPhongMaterial *>(material)) {
        phong->setDiffuse(color);
        return;
    }
    if (auto *alpha = qobject_cast<Qt3DExtras::QPhongAlphaMaterial *>(material))
        alpha->setDiffuse(color);
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
