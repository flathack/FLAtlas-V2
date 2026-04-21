// rendering/preview/ModelPreview.cpp - standalone dialog using the reusable 3D viewport

#include "ModelPreview.h"

#include "rendering/view3d/ModelViewport3D.h"

#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

namespace flatlas::rendering {

ModelPreview::ModelPreview(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("3D Model Preview"));
    resize(900, 700);

    auto *layout = new QVBoxLayout(this);

    auto *headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(tr("No model loaded"), this);
    headerLayout->addWidget(m_titleLabel, 1);

    auto *resetButton = new QPushButton(tr("Reset Camera"), this);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        if (m_viewport)
            m_viewport->resetView();
    });
    headerLayout->addWidget(resetButton);
    layout->addLayout(headerLayout);

    m_viewport = new ModelViewport3D(this);
    layout->addWidget(m_viewport, 1);
}

ModelPreview::~ModelPreview() = default;

void ModelPreview::loadModel(const QString &filePath)
{
    if (!m_viewport)
        return;

    m_titleLabel->setText(tr("Initializing 3D preview..."));
    QTimer::singleShot(0, this, [this, filePath]() {
        if (!m_viewport)
            return;

        QString errorMessage;
        if (m_viewport->loadModelFile(filePath, &errorMessage))
            m_titleLabel->setText(QFileInfo(filePath).fileName());
        else
            m_titleLabel->setText(errorMessage);
    });
}

QString ModelPreview::filePath() const
{
    return m_viewport ? m_viewport->currentFilePath() : QString();
}

bool ModelPreview::hasModel() const
{
    return m_viewport && m_viewport->hasModel();
}

} // namespace flatlas::rendering
