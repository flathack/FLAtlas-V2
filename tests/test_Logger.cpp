#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/Logger.h"

using flatlas::core::Logger;

class TestLogger : public QObject {
    Q_OBJECT
private slots:
    void logToFile()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString logPath = tmpDir.path() + QStringLiteral("/test.log");

        Logger::init(logPath);
        Logger::info(QStringLiteral("test"), QStringLiteral("hello world"));

        // Flush: Qt message handler writes immediately
        QFile file(logPath);
        QVERIFY(file.exists());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.contains(QStringLiteral("hello world")));
        QVERIFY(content.contains(QStringLiteral("test")));
    }

    void basicCallsDoNotCrash()
    {
        Logger::info(QStringLiteral("cat"), QStringLiteral("info msg"));
        Logger::warning(QStringLiteral("cat"), QStringLiteral("warn msg"));
        Logger::error(QStringLiteral("cat"), QStringLiteral("error msg"));
        Logger::debug(QStringLiteral("cat"), QStringLiteral("debug msg"));
    }
};

QTEST_MAIN(TestLogger)
#include "test_Logger.moc"
