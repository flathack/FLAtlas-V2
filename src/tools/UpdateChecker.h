#pragma once

#include <QObject>
#include <QJsonArray>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace flatlas::tools {

struct UpdateAsset {
    QString name;
    QUrl downloadUrl;
    QString packageType;

    bool isValid() const { return downloadUrl.isValid() && !packageType.isEmpty(); }
};

/// Ergebnis eines Update-Checks.
struct UpdateInfo {
    bool available = false;       ///< Neue Version verfuegbar?
    QString currentVersion;       ///< Aktuelle Version (lokal)
    QString latestVersion;        ///< Neueste Version (remote)
    QUrl downloadUrl;             ///< Download-URL des Release-Assets
    QString assetName;            ///< Gewaehltes Release-Asset
    QString packageType;          ///< zip oder installer
    QString architecture;         ///< Lokale Architektur
    QString releaseNotes;         ///< Release-Notes (Markdown)
    QString errorMessage;         ///< Fehlermeldung (leer bei Erfolg)
};

/// Prueft GitHub Releases auf neue Versionen.
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

    /// API-URL fuer latest release.
    static QUrl latestReleaseUrl();

    /// Versionsvergleich: true wenn remote > local.
    static bool isNewerVersion(const QString &local, const QString &remote);

    /// Lokale CPU-Architektur als Release-Asset-Suffix.
    static QString currentArchitecture();

    /// Bestes GitHub-Release-Asset fuer Windows und die lokale Architektur waehlen.
    static UpdateAsset selectBestAsset(const QJsonArray &assets,
                                       const QString &architecture = currentArchitecture());

signals:
    /// Wird emitted wenn der Check abgeschlossen ist.
    void updateCheckFinished(const UpdateInfo &info);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam = nullptr;
};

} // namespace flatlas::tools
