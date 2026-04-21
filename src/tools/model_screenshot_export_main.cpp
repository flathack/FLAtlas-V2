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
    parser.addOption(modelOption);
    parser.addOption(outputOption);
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
    if (!flatlas::tools::ModelScreenshotExporter::exportModelFileToJson(modelPath, outputPath, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << "\n";
        return 3;
    }

    return 0;
}