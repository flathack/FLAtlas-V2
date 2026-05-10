#include "UpdateChecker.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QVersionNumber>

namespace flatlas::tools {

namespace {
constexpr const char *kRepoOwner = "flathack";
constexpr const char *kRepoName  = "FLAtlas-V2";
constexpr const char *kAppVersion = "0.8.0";

bool hasAny(const QString &name, const QStringList &tokens)
{
    for (const QString &token : tokens) {
        if (name.contains(token))
            return true;
    }
    return false;
}

QString packageTypeForName(const QString &name)
{
    if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
        return QStringLiteral("zip");
    if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        return QStringLiteral("installer");
    return {};
}

QStringList architectureMatches(const QString &architecture)
{
    if (architecture == QStringLiteral("arm64"))
        return {QStringLiteral("arm64"), QStringLiteral("aarch64")};
    if (architecture == QStringLiteral("x86"))
        return {QStringLiteral("x86"), QStringLiteral("i386"), QStringLiteral("win32")};
    return {QStringLiteral("x64"), QStringLiteral("x86_64"), QStringLiteral("amd64"), QStringLiteral("win64")};
}

QStringList architectureConflicts(const QString &architecture)
{
    if (architecture == QStringLiteral("arm64"))
        return {QStringLiteral("x64"), QStringLiteral("x86_64"), QStringLiteral("amd64"), QStringLiteral("win64"),
                QStringLiteral("x86"), QStringLiteral("i386"), QStringLiteral("win32")};
    if (architecture == QStringLiteral("x86"))
        return {QStringLiteral("x64"), QStringLiteral("x86_64"), QStringLiteral("amd64"), QStringLiteral("win64"),
                QStringLiteral("arm64"), QStringLiteral("aarch64")};
    return {QStringLiteral("x86"), QStringLiteral("i386"), QStringLiteral("win32"),
            QStringLiteral("arm64"), QStringLiteral("aarch64")};
}
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
    auto clean = [](const QString &v) -> QString {
        return v.startsWith(QLatin1Char('v')) ? v.mid(1) : v;
    };

    const QVersionNumber localVer = QVersionNumber::fromString(clean(local));
    const QVersionNumber remoteVer = QVersionNumber::fromString(clean(remote));
    return remoteVer > localVer;
}

QString UpdateChecker::currentArchitecture()
{
    const QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch.contains(QStringLiteral("arm64")) || arch.contains(QStringLiteral("aarch64")))
        return QStringLiteral("arm64");
    if (arch.contains(QStringLiteral("i386")) || arch.contains(QStringLiteral("i686")) || arch == QStringLiteral("x86"))
        return QStringLiteral("x86");
    return QStringLiteral("x64");
}

UpdateAsset UpdateChecker::selectBestAsset(const QJsonArray &assets, const QString &architecture)
{
    UpdateAsset best;
    int bestScore = -1000;

    for (const QJsonValue &assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QString packageType = packageTypeForName(name);
        if (packageType.isEmpty())
            continue;

        const QUrl downloadUrl(asset.value(QStringLiteral("browser_download_url")).toString());
        if (!downloadUrl.isValid())
            continue;

        const QString lowerName = name.toLower();
        int score = packageType == QStringLiteral("zip") ? 30 : 20;
        if (hasAny(lowerName, {QStringLiteral("win"), QStringLiteral("windows")}))
            score += 25;

        const bool archConflict = hasAny(lowerName, architectureConflicts(architecture));
        const bool archMatch = hasAny(lowerName, architectureMatches(architecture)) && !archConflict;
        if (archMatch)
            score += 100;
        else if (archConflict)
            score -= 100;
        else
            score += 10;

        if (score > bestScore) {
            bestScore = score;
            best.name = name;
            best.downloadUrl = downloadUrl;
            best.packageType = packageType;
        }
    }

    return best;
}

void UpdateChecker::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    UpdateInfo info;
    info.currentVersion = currentVersion();
    info.architecture = currentArchitecture();

    if (reply->error() != QNetworkReply::NoError) {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 404) {
            info.errorMessage = tr("No FLAtlas release is available on GitHub yet.");
            info.releaseMissing = true;
        } else if (httpStatus == 403) {
            info.errorMessage = tr("GitHub rejected the update check. Please try again later.");
        } else if (httpStatus >= 400) {
            info.errorMessage = tr("GitHub update check failed with HTTP status %1.").arg(httpStatus);
        } else {
            info.errorMessage = tr("Could not check for updates: %1").arg(reply->errorString());
        }
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

    const UpdateAsset asset = selectBestAsset(obj.value(QStringLiteral("assets")).toArray(), info.architecture);
    info.downloadUrl = asset.downloadUrl;
    info.assetName = asset.name;
    info.packageType = asset.packageType;

    info.available = isNewerVersion(info.currentVersion, info.latestVersion);
    emit updateCheckFinished(info);
}

} // namespace flatlas::tools
