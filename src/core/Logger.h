#pragma once
// core/Logger.h – Strukturiertes Logging
// TODO Phase 1

#include <QString>
#include <QDebug>

namespace flatlas::core {

class Logger
{
public:
    static void info(const QString &category, const QString &message);
    static void warning(const QString &category, const QString &message);
    static void error(const QString &category, const QString &message);
};

} // namespace flatlas::core
