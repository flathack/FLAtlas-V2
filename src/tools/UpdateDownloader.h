#pragma once

#include <QObject>
#include <QUrl>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace flatlas::tools {

/// Lädt eine Datei (ZIP) von einer URL mit Fortschrittsanzeige.
class UpdateDownloader : public QObject
{
    Q_OBJECT

public:
    explicit UpdateDownloader(QObject *parent = nullptr);
    ~UpdateDownloader() override;

    /// Download starten. Speichert unter targetPath.
    void download(const QUrl &url, const QString &targetPath);

    /// Laufenden Download abbrechen.
    void cancel();

    /// Ist ein Download aktiv?
    bool isDownloading() const;

signals:
    /// Fortschritt in Prozent (0–100).
    void progressChanged(int percent);
    /// Download erfolgreich abgeschlossen.
    void downloadFinished(const QString &filePath);
    /// Fehler beim Download.
    void downloadFailed(const QString &errorMessage);

private slots:
    void onReadyRead();
    void onProgress(qint64 received, qint64 total);
    void onFinished();

private:
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_reply = nullptr;
    QFile *m_file = nullptr;
    QString m_targetPath;
};

} // namespace flatlas::tools
