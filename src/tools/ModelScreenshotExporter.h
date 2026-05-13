#pragma once

#include "infrastructure/io/CmpLoader.h"

#include <QVector>
#include <QVector3D>

class QString;

namespace flatlas::tools {

struct ScreenshotTriangle {
    QVector3D a;
    QVector3D b;
    QVector3D c;
};

class ModelScreenshotExporter
{
public:
    static QVector<ScreenshotTriangle> buildTriangles(const flatlas::infrastructure::DecodedModel &model);
    static bool exportTrianglesJson(const flatlas::infrastructure::DecodedModel &model,
                                    const QString &outPath,
                                    QString *errorMessage = nullptr);
    static bool exportModelFileToJson(const QString &modelPath,
                                      const QString &outPath,
                                      QString *errorMessage = nullptr);
    static bool exportTexturedTopViewPng(const QString &modelPath,
                                         const QString &outPath,
                                         int size = 384,
                                         QString *errorMessage = nullptr);
    static bool exportTexturedPerspectivePng(const QString &modelPath,
                                             const QString &outPath,
                                             int size = 768,
                                             float yawDegrees = 35.0f,
                                             float pitchDegrees = 18.0f,
                                             QString *errorMessage = nullptr);
};

} // namespace flatlas::tools
