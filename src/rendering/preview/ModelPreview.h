#pragma once
// rendering/preview/ModelPreview.h - standalone dialog using the reusable 3D viewport

#include <QDialog>

class QLabel;

namespace flatlas::rendering {

class ModelViewport3D;

class ModelPreview : public QDialog {
    Q_OBJECT
public:
    explicit ModelPreview(QWidget *parent = nullptr);
    ~ModelPreview() override;

    void loadModel(const QString &filePath);
    QString filePath() const;
    bool hasModel() const;

private:
    ModelViewport3D *m_viewport = nullptr;
    QLabel *m_titleLabel = nullptr;
};

} // namespace flatlas::rendering

