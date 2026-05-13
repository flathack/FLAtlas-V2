#include "tools/ModelScreenshotExporter.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>

#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("flatlas_model_screenshot_exporter"));
    app.setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Export decoded FLAtlas V2 model triangles as JSON for screenshot rendering."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption modelOption(QStringList{QStringLiteral("m"), QStringLiteral("model")},
                                         QStringLiteral("Input CMP/3DB model path."),
                                         QStringLiteral("model"));
    const QCommandLineOption outputOption(QStringList{QStringLiteral("o"), QStringLiteral("output")},
                                          QStringLiteral("Output JSON path."),
                                          QStringLiteral("output"));
    const QCommandLineOption pngOption(QStringList{QStringLiteral("p"), QStringLiteral("png")},
                                       QStringLiteral("Export a textured transparent top-view PNG instead of JSON."));
    const QCommandLineOption perspectiveOption(QStringList{QStringLiteral("perspective")},
                                               QStringLiteral("Render PNG from an angled perspective camera instead of top view."));
    const QCommandLineOption sizeOption(QStringList{QStringLiteral("s"), QStringLiteral("size")},
                                        QStringLiteral("PNG size in pixels. Defaults to 384."),
                                        QStringLiteral("size"),
                                        QStringLiteral("384"));
    const QCommandLineOption yawOption(QStringList{QStringLiteral("yaw")},
                                       QStringLiteral("Perspective yaw in degrees. Defaults to 35."),
                                       QStringLiteral("degrees"),
                                       QStringLiteral("35"));
    const QCommandLineOption pitchOption(QStringList{QStringLiteral("pitch")},
                                         QStringLiteral("Perspective pitch in degrees. Defaults to 18."),
                                         QStringLiteral("degrees"),
                                         QStringLiteral("18"));
    parser.addOption(modelOption);
    parser.addOption(outputOption);
    parser.addOption(pngOption);
    parser.addOption(perspectiveOption);
    parser.addOption(sizeOption);
    parser.addOption(yawOption);
    parser.addOption(pitchOption);
    parser.process(app);

    const QString modelPath = parser.value(modelOption).trimmed();
    const QString outputPath = parser.value(outputOption).trimmed();
    if (modelPath.isEmpty() || outputPath.isEmpty()) {
        std::cerr << "Both --model and --output are required.\n";
        parser.showHelp(1);
    }

    if (!QFileInfo::exists(modelPath)) {
        std::cerr << "Model file not found: " << modelPath.toStdString() << "\n";
        return 2;
    }

    QString errorMessage;
    bool ok = false;
    if (parser.isSet(pngOption)) {
        bool sizeOk = false;
        const int size = parser.value(sizeOption).toInt(&sizeOk);
        if (parser.isSet(perspectiveOption)) {
            bool yawOk = false;
            bool pitchOk = false;
            const float yaw = parser.value(yawOption).toFloat(&yawOk);
            const float pitch = parser.value(pitchOption).toFloat(&pitchOk);
            ok = flatlas::tools::ModelScreenshotExporter::exportTexturedPerspectivePng(
                modelPath,
                outputPath,
                sizeOk ? size : 768,
                yawOk ? yaw : 35.0f,
                pitchOk ? pitch : 18.0f,
                &errorMessage);
        } else {
            ok = flatlas::tools::ModelScreenshotExporter::exportTexturedTopViewPng(
                modelPath, outputPath, sizeOk ? size : 384, &errorMessage);
        }
    } else {
        ok = flatlas::tools::ModelScreenshotExporter::exportModelFileToJson(modelPath, outputPath, &errorMessage);
    }
    if (!ok) {
        std::cerr << errorMessage.toStdString() << "\n";
        return 3;
    }

    return 0;
}
