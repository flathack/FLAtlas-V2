#include "Logger.h"
#include <QLoggingCategory>

namespace flatlas::core {

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
