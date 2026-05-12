#include "GuideRemoteFetcher.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace flatlas::infrastructure::guide {

GuideRemoteFetcher::GuideRemoteFetcher(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

GuideRemoteFetcher::~GuideRemoteFetcher()
{
    cancel();
}

void GuideRemoteFetcher::fetch(const QUrl &manifestUrl)
{
    if (isFetching()) {
        emit failed(tr("A guide download is already in progress."));
        return;
    }

    if (!manifestUrl.isValid()) {
        emit failed(tr("Guide manifest URL is invalid."));
        return;
    }

    m_manifestUrl = manifestUrl;
    m_baseUrl = manifestUrl.resolved(QUrl(QStringLiteral(".")));
    m_package = {};
    m_entries.clear();
    m_nextArticleIndex = 0;

    startRequest(manifestUrl, Stage::Manifest);
}

void GuideRemoteFetcher::cancel()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_stage = Stage::Idle;
}

bool GuideRemoteFetcher::isFetching() const
{
    return m_reply != nullptr;
}

GuideSyncService::SyncPackage GuideRemoteFetcher::package() const
{
    return m_package;
}

void GuideRemoteFetcher::startRequest(const QUrl &url, Stage stage)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FLAtlas-V2"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_stage = stage;
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &GuideRemoteFetcher::onReplyFinished);
}

void GuideRemoteFetcher::onReplyFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;

    const Stage finishedStage = m_stage;
    m_stage = Stage::Idle;

    if (!reply)
        return;

    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        fail(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    if (finishedStage == Stage::Manifest) {
        QString error;
        const auto catalog = m_reader.readCatalog(data, &error);
        if (!error.isEmpty()) {
            fail(error);
            return;
        }

        m_package.manifestJson = data;
        m_entries = catalog.articles;
        m_nextArticleIndex = 0;
        fetchNextArticle();
        return;
    }

    if (finishedStage == Stage::Article) {
        const int articleIndex = m_nextArticleIndex - 1;
        if (articleIndex < 0 || articleIndex >= m_entries.size()) {
            fail(tr("Guide article download state is invalid."));
            return;
        }

        m_package.articlesByPath.insert(m_entries.at(articleIndex).path, data);
        fetchNextArticle();
    }
}

void GuideRemoteFetcher::fail(const QString &errorMessage)
{
    cancel();
    emit failed(errorMessage);
}

void GuideRemoteFetcher::fetchNextArticle()
{
    if (m_nextArticleIndex >= m_entries.size()) {
        emit finished();
        return;
    }

    const auto entry = m_entries.at(m_nextArticleIndex);
    ++m_nextArticleIndex;
    startRequest(m_baseUrl.resolved(QUrl(entry.path)), Stage::Article);
}

} // namespace flatlas::infrastructure::guide
