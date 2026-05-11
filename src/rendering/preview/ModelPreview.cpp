// rendering/preview/ModelPreview.cpp - standalone dialog using the reusable 3D viewport

#include "ModelPreview.h"

#include "rendering/view3d/ModelViewport3D.h"

#include <QFileInfo>
#include <QCheckBox>
#include <QColorDialog>
#include <QDial>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
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

    auto *texturesCheck = new QCheckBox(tr("Textures"), this);
    texturesCheck->setChecked(true);
    texturesCheck->setToolTip(tr("Show model textures."));
    headerLayout->addWidget(texturesCheck);

    auto *lightDial = new QDial(this);
    lightDial->setRange(0, 359);
    lightDial->setValue(45);
    lightDial->setWrapping(true);
    lightDial->setNotchesVisible(true);
    lightDial->setFixedSize(42, 42);
    lightDial->setToolTip(tr("Rotate light source."));
    headerLayout->addWidget(new QLabel(tr("Light"), this));
    headerLayout->addWidget(lightDial);

    auto *intensitySlider = new QSlider(Qt::Horizontal, this);
    intensitySlider->setRange(0, 300);
    intensitySlider->setValue(110);
    intensitySlider->setFixedWidth(90);
    intensitySlider->setToolTip(tr("Light intensity."));
    headerLayout->addWidget(new QLabel(tr("Intensity"), this));
    headerLayout->addWidget(intensitySlider);

    auto *colorButton = new QPushButton(tr("Color"), this);
    colorButton->setToolTip(tr("Light color."));
    QColor lightColor = Qt::white;
    auto updateColorButton = [colorButton](const QColor &color) {
        colorButton->setStyleSheet(QStringLiteral("QPushButton { background:%1; }").arg(color.name()));
    };
    updateColorButton(lightColor);
    headerLayout->addWidget(colorButton);

    auto *resetButton = new QPushButton(tr("Reset Camera"), this);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        if (m_viewport)
            m_viewport->resetView();
    });
    headerLayout->addWidget(resetButton);
    layout->addLayout(headerLayout);

    m_viewport = new ModelViewport3D(this);
    connect(texturesCheck, &QCheckBox::toggled, m_viewport, &ModelViewport3D::setTexturesVisible);
    connect(lightDial, &QDial::valueChanged, m_viewport, [this](int value) {
        if (m_viewport)
            m_viewport->setLightAzimuth(static_cast<float>(value));
    });
    connect(intensitySlider, &QSlider::valueChanged, m_viewport, [this](int value) {
        if (m_viewport)
            m_viewport->setLightIntensity(static_cast<float>(value) / 100.0f);
    });
    connect(colorButton, &QPushButton::clicked, this, [this, lightColor, updateColorButton]() mutable {
        const QColor chosen = QColorDialog::getColor(lightColor, this, tr("Light Color"));
        if (!chosen.isValid())
            return;
        lightColor = chosen;
        updateColorButton(lightColor);
        if (m_viewport)
            m_viewport->setLightColor(lightColor);
    });
    m_viewport->setTexturesVisible(texturesCheck->isChecked());
    m_viewport->setLightAzimuth(static_cast<float>(lightDial->value()));
    m_viewport->setLightIntensity(static_cast<float>(intensitySlider->value()) / 100.0f);
    m_viewport->setLightColor(lightColor);
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
