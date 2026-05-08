#include "Logger.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace flatlas::core {

static QFile s_logFile;
static QMutex s_logMutex;
static bool s_fileLoggingEnabled = true;

void Logger::init(const QString &logFilePath)
{
    QMutexLocker locker(&s_logMutex);

    if (s_logFile.isOpen())
        s_logFile.close();

    if (!logFilePath.isEmpty()) {
        QDir().mkpath(QFileInfo(logFilePath).absolutePath());
        s_logFile.setFileName(logFilePath);
        s_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    qInstallMessageHandler(messageHandler);
}

void Logger::setFileLogging(bool enabled)
{
    s_fileLoggingEnabled = enabled;
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (msg.contains(QStringLiteral("Setting a new default format with a different version or profile after the global shared context is created")))
        return;

    static const char *levels[] = {"DEBUG", "WARNING", "CRITICAL", "FATAL", "INFO"};
    const char *level = (type >= 0 && type <= 4) ? levels[type] : "UNKNOWN";
    const QString cat = context.category ? QString::fromUtf8(context.category) : QStringLiteral("default");
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString formatted = QStringLiteral("[%1] [%2] [%3] %4").arg(timestamp, QLatin1String(level), cat, msg);

    QMutexLocker locker(&s_logMutex);
    QTextStream errStream(stderr);
    errStream << formatted << Qt::endl;

    if (s_fileLoggingEnabled && s_logFile.isOpen()) {
        QTextStream fileStream(&s_logFile);
        fileStream << formatted << Qt::endl;
        s_logFile.flush();
    }
}

void Logger::debug(const QString &category, const QString &message)
{
    qDebug().noquote() << QStringLiteral("[%1] %2").arg(category, message);
}

void Logger::info(const QString &category, const QString &message)
{
    qInfo().noquote() << QStringLiteral("[%1] %2").arg(category, message);
}

void Logger::warning(const QString &category, const QString &message)
{
    qWarning().noquote() << QStringLiteral("[%1] %2").arg(category, message);
}

void Logger::error(const QString &category, const QString &message)
{
    qCritical().noquote() << QStringLiteral("[%1] %2").arg(category, message);
}

} // namespace flatlas::core
