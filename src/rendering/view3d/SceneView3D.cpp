#include "SceneView3D.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::rendering {
SceneView3D::SceneView3D(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *placeholder = new QLabel(tr("3D View – wird in Phase 7 implementiert"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);
    // TODO Phase 7: Qt3DWindow einbetten
}
} // namespace flatlas::rendering
