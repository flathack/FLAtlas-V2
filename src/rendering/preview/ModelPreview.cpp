#include "ModelPreview.h"
namespace flatlas::rendering {
ModelPreview::ModelPreview(QWidget *parent) : QDialog(parent) { setWindowTitle(tr("Model Preview")); }
void ModelPreview::loadModel(const QString &) {} // TODO Phase 16
} // namespace flatlas::rendering
