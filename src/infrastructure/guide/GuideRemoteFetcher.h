#pragma once

#include "domain/guide/GuideArticle.h"
#include "infrastructure/guide/GuideJsonReader.h"
#include "infrastructure/guide/GuideSyncService.h"

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace flatlas::infrastructure::guide {

class GuideRemoteFetcher : public QObject
{
    Q_OBJECT

public:
    explicit GuideRemoteFetcher(QObject *parent = nullptr);
    ~GuideRemoteFetcher() override;

    void fetch(const QUrl &manifestUrl);
    void cancel();

    bool isFetching() const;
    GuideSyncService::SyncPackage package() const;

signals:
    void finished();
    void failed(const QString &errorMessage);

private:
    enum class Stage {
        Idle,
        Manifest,
        Article
    };

    void startRequest(const QUrl &url, Stage stage);
    void onReplyFinished();
    void fail(const QString &errorMessage);
    void fetchNextArticle();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    Stage m_stage = Stage::Idle;
    QUrl m_manifestUrl;
    QUrl m_baseUrl;
    GuideJsonReader m_reader;
    GuideSyncService::SyncPackage m_package;
    QVector<flatlas::domain::guide::GuideCatalogEntry> m_entries;
    int m_nextArticleIndex = 0;
};

} // namespace flatlas::infrastructure::guide
