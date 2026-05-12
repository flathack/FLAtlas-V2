#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::domain::guide {

struct GuideBlockItem {
    QString path;
    QString purpose;
    QString text;
};

struct GuideBlock {
    QString type;
    QString title;
    QString text;
    QString language;
    QStringList items;
    QVector<GuideBlockItem> fileItems;
    QStringList articleIds;
};

struct GuideArticle {
    int schema = 0;
    QString id;
    QString language;
    QString title;
    QString summary;
    QString category;
    QStringList tags;
    QStringList contexts;
    QString updatedAt;
    QVector<GuideBlock> blocks;
};

struct GuideCatalogEntry {
    QString id;
    QString language;
    QString title;
    QString summary;
    QString category;
    QStringList tags;
    QStringList contexts;
    QString level;
    QString path;
    int version = 0;
    QString updatedAt;
    QString sha256;
};

struct GuideCatalog {
    int schema = 0;
    QString catalogVersion;
    QString defaultLanguage;
    QStringList languages;
    QString generatedAt;
    QVector<GuideCatalogEntry> articles;
};

} // namespace flatlas::domain::guide
