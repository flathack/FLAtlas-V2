#pragma once
// core/Logger.h – Strukturiertes Logging

#include <QString>
#include <QDebug>

namespace flatlas::core {

class Logger
{
public:
    static void init(const QString &logFilePath = {});
    static void setFileLogging(bool enabled);
    static void debug(const QString &category, const QString &message);
    static void info(const QString &category, const QString &message);
    static void warning(const QString &category, const QString &message);
    static void error(const QString &category, const QString &message);

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
};

} // namespace flatlas::core
