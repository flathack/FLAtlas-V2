#include "UpdateDownloader.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>

namespace flatlas::tools {

UpdateDownloader::UpdateDownloader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

UpdateDownloader::~UpdateDownloader()
{
    cancel();
}

void UpdateDownloader::download(const QUrl &url, const QString &targetPath)
{
    if (m_reply) {
        emit downloadFailed(tr("A download is already in progress"));
        return;
    }

    m_targetPath = targetPath;
    m_file = new QFile(targetPath, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit downloadFailed(tr("Cannot open file for writing: %1").arg(targetPath));
        delete m_file;
        m_file = nullptr;
        return;
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FLAtlas-V2"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &UpdateDownloader::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &UpdateDownloader::onProgress);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateDownloader::onFinished);
}

void UpdateDownloader::cancel()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        m_file->close();
        m_file->remove();
        delete m_file;
        m_file = nullptr;
    }
}

bool UpdateDownloader::isDownloading() const
{
    return m_reply != nullptr;
}

void UpdateDownloader::onReadyRead()
{
    if (m_file && m_reply)
        m_file->write(m_reply->readAll());
}

void UpdateDownloader::onProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        const int pct = static_cast<int>(received * 100 / total);
        emit progressChanged(pct);
    }
}

void UpdateDownloader::onFinished()
{
    if (!m_reply)
        return;

    const bool hadError = m_reply->error() != QNetworkReply::NoError;
    const QString errorMsg = m_reply->errorString();

    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_file) {
        m_file->close();

        if (hadError) {
            m_file->remove();
            delete m_file;
            m_file = nullptr;
            emit downloadFailed(errorMsg);
        } else {
            delete m_file;
            m_file = nullptr;
            emit downloadFinished(m_targetPath);
        }
    }
}

} // namespace flatlas::tools
