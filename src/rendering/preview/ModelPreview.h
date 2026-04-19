#pragma once
// rendering/preview/ModelPreview.h – 3D-Modell-Vorschau
// TODO Phase 16
#include <QDialog>
namespace flatlas::rendering {
class ModelPreview : public QDialog {
    Q_OBJECT
public:
    explicit ModelPreview(QWidget *parent = nullptr);
    void loadModel(const QString &filePath);
};
} // namespace flatlas::rendering
