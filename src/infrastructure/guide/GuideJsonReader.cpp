#include "GuideJsonReader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace flatlas::infrastructure::guide {
namespace {

QStringList readStringList(const QJsonValue &value)
{
    QStringList result;
    const QJsonArray array = value.toArray();
    result.reserve(array.size());
    for (const QJsonValue &item : array) {
        if (item.isString())
            result.append(item.toString());
    }
    return result;
}

bool parseObjectDocument(const QByteArray &json, QJsonObject &object, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage)
            *errorMessage = parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Guide JSON root must be an object.");
        return false;
    }

    object = doc.object();
    return true;
}

flatlas::domain::guide::GuideCatalogEntry readCatalogEntry(const QJsonObject &object)
{
    flatlas::domain::guide::GuideCatalogEntry entry;
    entry.id = object.value(QStringLiteral("id")).toString();
    entry.language = object.value(QStringLiteral("language")).toString();
    entry.title = object.value(QStringLiteral("title")).toString();
    entry.summary = object.value(QStringLiteral("summary")).toString();
    entry.category = object.value(QStringLiteral("category")).toString();
    entry.tags = readStringList(object.value(QStringLiteral("tags")));
    entry.contexts = readStringList(object.value(QStringLiteral("contexts")));
    entry.level = object.value(QStringLiteral("level")).toString();
    entry.path = object.value(QStringLiteral("path")).toString();
    entry.version = object.value(QStringLiteral("version")).toInt();
    entry.updatedAt = object.value(QStringLiteral("updatedAt")).toString();
    entry.sha256 = object.value(QStringLiteral("sha256")).toString();
    return entry;
}

flatlas::domain::guide::GuideBlock readBlock(const QJsonObject &object)
{
    flatlas::domain::guide::GuideBlock block;
    block.type = object.value(QStringLiteral("type")).toString();
    block.title = object.value(QStringLiteral("title")).toString();
    block.text = object.value(QStringLiteral("text")).toString();
    block.language = object.value(QStringLiteral("language")).toString();
    block.items = readStringList(object.value(QStringLiteral("items")));
    block.articleIds = readStringList(object.value(QStringLiteral("articleIds")));

    const QJsonArray fileItems = object.value(QStringLiteral("items")).toArray();
    block.fileItems.reserve(fileItems.size());
    for (const QJsonValue &value : fileItems) {
        if (!value.isObject())
            continue;

        const QJsonObject itemObject = value.toObject();
        flatlas::domain::guide::GuideBlockItem item;
        item.path = itemObject.value(QStringLiteral("path")).toString();
        item.purpose = itemObject.value(QStringLiteral("purpose")).toString();
        item.text = itemObject.value(QStringLiteral("text")).toString();
        block.fileItems.append(item);
    }

    return block;
}

} // namespace

flatlas::domain::guide::GuideCatalog GuideJsonReader::readCatalog(const QByteArray &json, QString *errorMessage) const
{
    QJsonObject object;
    if (!parseObjectDocument(json, object, errorMessage))
        return {};

    flatlas::domain::guide::GuideCatalog catalog;
    catalog.schema = object.value(QStringLiteral("schema")).toInt();
    catalog.catalogVersion = object.value(QStringLiteral("catalogVersion")).toString();
    catalog.defaultLanguage = object.value(QStringLiteral("defaultLanguage")).toString();
    catalog.languages = readStringList(object.value(QStringLiteral("languages")));
    catalog.generatedAt = object.value(QStringLiteral("generatedAt")).toString();

    const QJsonArray articles = object.value(QStringLiteral("articles")).toArray();
    catalog.articles.reserve(articles.size());
    for (const QJsonValue &articleValue : articles) {
        if (articleValue.isObject())
            catalog.articles.append(readCatalogEntry(articleValue.toObject()));
    }

    return catalog;
}

flatlas::domain::guide::GuideArticle GuideJsonReader::readArticle(const QByteArray &json, QString *errorMessage) const
{
    QJsonObject object;
    if (!parseObjectDocument(json, object, errorMessage))
        return {};

    flatlas::domain::guide::GuideArticle article;
    article.schema = object.value(QStringLiteral("schema")).toInt();
    article.id = object.value(QStringLiteral("id")).toString();
    article.language = object.value(QStringLiteral("language")).toString();
    article.title = object.value(QStringLiteral("title")).toString();
    article.summary = object.value(QStringLiteral("summary")).toString();
    article.category = object.value(QStringLiteral("category")).toString();
    article.tags = readStringList(object.value(QStringLiteral("tags")));
    article.contexts = readStringList(object.value(QStringLiteral("contexts")));
    article.updatedAt = object.value(QStringLiteral("updatedAt")).toString();

    const QJsonArray blocks = object.value(QStringLiteral("blocks")).toArray();
    article.blocks.reserve(blocks.size());
    for (const QJsonValue &blockValue : blocks) {
        if (blockValue.isObject())
            article.blocks.append(readBlock(blockValue.toObject()));
    }

    return article;
}

} // namespace flatlas::infrastructure::guide
