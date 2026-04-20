#pragma once
// infrastructure/io/DebugCapture.h – Capture OutputDebugString from a child process

#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace flatlas::infrastructure {

/// Captures debug output (OutputDebugString) from a launched process.
/// On Windows, uses a named shared-memory / event protocol (DBWIN_BUFFER).
/// Falls back to stdout/stderr capture on non-Windows platforms.
class DebugCapture : public QObject {
    Q_OBJECT
public:
    explicit DebugCapture(QObject *parent = nullptr);
    ~DebugCapture() override;

    /// Start capturing debug output from the given executable.
    bool start(const QString &exePath, const QStringList &args = {});

    /// Stop capturing and terminate the process.
    void stop();

    /// Whether the capture is currently running.
    bool isRunning() const;

    /// Return all captured lines so far.
    QStringList capturedLines() const { return m_lines; }

    /// Clear the captured buffer.
    void clearLines() { m_lines.clear(); }

signals:
    /// Emitted for each debug output line received.
    void lineReceived(const QString &line);

    /// Emitted when the process finishes.
    void processFinished(int exitCode);

private:
    void onReadyRead();

    QProcess   *m_process = nullptr;
    QStringList m_lines;
};

} // namespace flatlas::infrastructure
