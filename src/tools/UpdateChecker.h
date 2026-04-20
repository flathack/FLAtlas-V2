#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace flatlas::tools {

/// Ergebnis eines Update-Checks.
struct UpdateInfo {
    bool available = false;       ///< Neue Version verfügbar?
    QString currentVersion;       ///< Aktuelle Version (lokal)
    QString latestVersion;        ///< Neueste Version (remote)
    QUrl downloadUrl;             ///< ZIP-Download-URL
    QString releaseNotes;         ///< Release-Notes (Markdown)
    QString errorMessage;         ///< Fehlermeldung (leer bei Erfolg)
};

/// Prüft GitHub Releases auf neue Versionen.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    /// Asynchronen Update-Check starten.
    void checkForUpdates();

    /// Aktuell laufende Version.
    static QString currentVersion();

    /// GitHub-Repository-Owner/Name.
    static QString repoOwner();
    static QString repoName();

    /// API-URL für latest release.
    static QUrl latestReleaseUrl();

    /// Versionsvergleich: true wenn remote > local.
    static bool isNewerVersion(const QString &local, const QString &remote);

signals:
    /// Wird emitted wenn der Check abgeschlossen ist.
    void updateCheckFinished(const UpdateInfo &info);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam = nullptr;
};

} // namespace flatlas::tools
