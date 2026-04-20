// infrastructure/io/DebugCapture.cpp – Capture debug output from a child process

#include "DebugCapture.h"
#include <QProcess>

namespace flatlas::infrastructure {

DebugCapture::DebugCapture(QObject *parent)
    : QObject(parent)
{
}

DebugCapture::~DebugCapture()
{
    stop();
}

bool DebugCapture::start(const QString &exePath, const QStringList &args)
{
    if (m_process)
        return false;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &DebugCapture::onReadyRead);
    connect(m_process, &QProcess::finished,
            this, [this](int exitCode, QProcess::ExitStatus) {
                emit processFinished(exitCode);
            });

    m_process->start(exePath, args);
    return m_process->waitForStarted(5000);
}

void DebugCapture::stop()
{
    if (!m_process)
        return;

    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }

    m_process->deleteLater();
    m_process = nullptr;
}

bool DebugCapture::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void DebugCapture::onReadyRead()
{
    while (m_process->canReadLine()) {
        const QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (!line.isEmpty()) {
            m_lines.append(line);
            emit lineReceived(line);
        }
    }
}

} // namespace flatlas::infrastructure
