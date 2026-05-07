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
    m_entities[nickname].append(info);

    // Add object picker
    auto *picker = new Qt3DRender::QObjectPicker(entity);
    picker->setHoverEnabled(true);
    entity->addComponent(picker);

    connect(picker, &Qt3DRender::QObjectPicker::clicked,
            this, [this, nickname](Qt3DRender::QPickEvent *event) {
        onPicked(event, nickname);
    });
    connect(picker, &Qt3DRender::QObjectPicker::entered,
            this, [this, nickname]() {
        if (!m_pickingSuppressed)
            setHovered(nickname);
    });
    connect(picker, &Qt3DRender::QObjectPicker::exited,
            this, [this, nickname]() {
        if (m_hoveredNickname == nickname)
            setHovered(QString());
    });
}

void SelectionManager::clear()
{
    m_selectedNickname.clear();
    m_hoveredNickname.clear();
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

void SelectionManager::onPicked(Qt3DRender::QPickEvent *event, const QString &nickname)
{
    if (m_pickingSuppressed)
        return;
    if (!event || event->button() != Qt3DRender::QPickEvent::LeftButton)
        return;
    select(nickname);
}

void SelectionManager::setHovered(const QString &nickname)
{
    if (m_hoveredNickname == nickname)
        return;
    if (!m_hoveredNickname.isEmpty() && m_hoveredNickname != m_selectedNickname)
        applyHoverHighlight(m_hoveredNickname, false);
    m_hoveredNickname = nickname;
    if (!m_hoveredNickname.isEmpty() && m_hoveredNickname != m_selectedNickname)
        applyHoverHighlight(m_hoveredNickname, true);
    emit objectHovered(m_hoveredNickname);
}

void SelectionManager::applyHighlight(const QString &nickname, bool highlighted)
{
    const auto infos = m_entities.value(nickname);
    if (infos.isEmpty())
        return;

    for (const EntityInfo &info : infos) {
        if (highlighted)
            setDiffuseColor(info.material, m_highlightColor);
        else
            setDiffuseColor(info.material, nickname == m_hoveredNickname ? m_hoverColor : info.originalDiffuse);
    }
}

void SelectionManager::applyHoverHighlight(const QString &nickname, bool highlighted)
{
    const auto infos = m_entities.value(nickname);
    if (infos.isEmpty())
        return;
    for (const EntityInfo &info : infos)
        setDiffuseColor(info.material, highlighted ? m_hoverColor : info.originalDiffuse);
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
