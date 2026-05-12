#pragma once

#include <QByteArray>
#include <QString>

namespace flatlas::infrastructure::guide {

class GuideCache
{
public:
    explicit GuideCache(QString rootPath);

    static GuideCache forAppDataLocation();

    QString rootPath() const;
    QString catalogPath(const QString &catalogVersion) const;
    QString articlePath(const QString &catalogVersion, const QString &relativePath) const;
    QString activeCatalogVersion() const;

    bool storeCatalog(const QString &catalogVersion, const QByteArray &data, QString *errorMessage = nullptr) const;
    bool storeArticle(const QString &catalogVersion,
                      const QString &relativePath,
                      const QByteArray &data,
                      const QString &expectedSha256,
                      QString *errorMessage = nullptr) const;

    QByteArray loadCatalog(const QString &catalogVersion, QString *errorMessage = nullptr) const;
    QByteArray loadArticle(const QString &catalogVersion,
                           const QString &relativePath,
                           const QString &expectedSha256,
                           QString *errorMessage = nullptr) const;

    bool activateCatalogVersion(const QString &catalogVersion, QString *errorMessage = nullptr) const;
    static QString sha256Hex(const QByteArray &data);

private:
    QString m_rootPath;
};

} // namespace flatlas::infrastructure::guide
