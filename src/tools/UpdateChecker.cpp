#include "UpdateChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVersionNumber>

namespace flatlas::tools {

namespace {
constexpr const char *kRepoOwner = "flathack";
constexpr const char *kRepoName  = "FLAtlas-V2";
constexpr const char *kAppVersion = "2.0.0";
} // anonymous

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::checkForUpdates()
{
    QNetworkRequest req(latestReleaseUrl());
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FLAtlas-V2"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    m_nam->get(req);
}

QString UpdateChecker::currentVersion()
{
    return QString::fromLatin1(kAppVersion);
}

QString UpdateChecker::repoOwner()
{
    return QString::fromLatin1(kRepoOwner);
}

QString UpdateChecker::repoName()
{
    return QString::fromLatin1(kRepoName);
}

QUrl UpdateChecker::latestReleaseUrl()
{
    return QUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                    .arg(repoOwner(), repoName()));
}

bool UpdateChecker::isNewerVersion(const QString &local, const QString &remote)
{
    // Strip leading 'v' if present
    auto clean = [](const QString &v) -> QString {
        return v.startsWith(QLatin1Char('v')) ? v.mid(1) : v;
    };

    const QVersionNumber localVer = QVersionNumber::fromString(clean(local));
    const QVersionNumber remoteVer = QVersionNumber::fromString(clean(remote));
    return remoteVer > localVer;
}

void UpdateChecker::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    UpdateInfo info;
    info.currentVersion = currentVersion();

    if (reply->error() != QNetworkReply::NoError) {
        info.errorMessage = reply->errorString();
        emit updateCheckFinished(info);
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        info.errorMessage = tr("Invalid response from GitHub API");
        emit updateCheckFinished(info);
        return;
    }

    const QJsonObject obj = doc.object();
    info.latestVersion = obj.value(QStringLiteral("tag_name")).toString();
    info.releaseNotes = obj.value(QStringLiteral("body")).toString();

    // Suche nach ZIP-Asset für Windows
    const QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
            && name.contains(QStringLiteral("win"), Qt::CaseInsensitive)) {
            info.downloadUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            break;
        }
    }

    // Fallback: erstes ZIP-Asset
    if (!info.downloadUrl.isValid()) {
        for (const QJsonValue &assetVal : assets) {
            const QJsonObject asset = assetVal.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                info.downloadUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
                break;
            }
        }
    }

    info.available = isNewerVersion(info.currentVersion, info.latestVersion);
    emit updateCheckFinished(info);
}

} // namespace flatlas::tools
