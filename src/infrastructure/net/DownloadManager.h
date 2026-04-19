#pragma once
// infrastructure/net/DownloadManager.h – HTTP-Downloads
// TODO Phase 22
#include <QObject>
#include <QString>
#include <QUrl>
namespace flatlas::infrastructure {
class DownloadManager : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(QObject *parent = nullptr) : QObject(parent) {}
    void download(const QUrl &url, const QString &outputPath);
signals:
    void progress(qint64 bytesReceived, qint64 bytesTotal);
    void finished(const QString &outputPath);
    void failed(const QString &error);
};
} // namespace flatlas::infrastructure
