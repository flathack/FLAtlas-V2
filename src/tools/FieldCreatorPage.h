#pragma once

#include "FieldTemplateGenerator.h"

#include <QWidget>

class QCheckBox;
class QColor;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextEdit;

namespace flatlas::tools {

class FieldPreviewWidget;

class FieldCreatorPage : public QWidget
{
    Q_OBJECT

public:
    explicit FieldCreatorPage(QWidget *parent = nullptr);

private:
    void buildUi();
    void loadAssetsForKind();
    void applyCurrentPreset();
    void applyTemplateToUi(const FieldTemplate &field);
    void updateTemplateFromUi();
    void refreshPreviews();
    void refreshPlacementTable();
    void addManualObject();
    void autoDistributeObjects();
    void loadTemplate();
    void saveTemplate();
    void updateAssetPreview();
    QVector<FieldAsset> selectedAssets() const;
    FieldTemplateKind currentKind() const;

    FieldTemplate m_template;
    QVector<FieldTemplate> m_presets;
    QVector<FieldAsset> m_assets;

    QComboBox *m_kindCombo = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QLineEdit *m_fileNameEdit = nullptr;
    QLineEdit *m_texturePanelsEdit = nullptr;
    QLineEdit *m_billboardShapeEdit = nullptr;
    QLineEdit *m_spacedustEdit = nullptr;
    QLineEdit *m_musicEdit = nullptr;
    QSpinBox *m_propertyFlagsSpin = nullptr;
    QSpinBox *m_visitSpin = nullptr;
    QSpinBox *m_damageSpin = nullptr;
    QSpinBox *m_cubeSizeSpin = nullptr;
    QSpinBox *m_fillDistanceSpin = nullptr;
    QDoubleSpinBox *m_emptyCubeSpin = nullptr;
    QSpinBox *m_billboardCountSpin = nullptr;
    QSpinBox *m_dynamicCountSpin = nullptr;
    QSpinBox *m_fogDistanceSpin = nullptr;
    QSpinBox *m_puffCountSpin = nullptr;
    QLineEdit *m_primaryColorEdit = nullptr;
    QLineEdit *m_ambientColorEdit = nullptr;
    QLineEdit *m_fogColorEdit = nullptr;
    QListWidget *m_assetList = nullptr;
    QTableWidget *m_placementTable = nullptr;
    QLineEdit *m_manualAssetEdit = nullptr;
    QDoubleSpinBox *m_xSpin = nullptr;
    QDoubleSpinBox *m_ySpin = nullptr;
    QDoubleSpinBox *m_zSpin = nullptr;
    QSpinBox *m_rotateXSpin = nullptr;
    QSpinBox *m_rotateYSpin = nullptr;
    QSpinBox *m_rotateZSpin = nullptr;
    QCheckBox *m_mineRoleCheck = nullptr;
    QSpinBox *m_autoCountSpin = nullptr;
    QSpinBox *m_seedSpin = nullptr;
    QComboBox *m_spreadCombo = nullptr;
    FieldPreviewWidget *m_assetPreview = nullptr;
    FieldPreviewWidget *m_preview = nullptr;
    QTextEdit *m_linkPreview = nullptr;
    QTextEdit *m_iniPreview = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace flatlas::tools
