#pragma once
// core/Theme.h – QSS palette-based theming with 4 built-in themes

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace flatlas::core {

using Palette = QHash<QString, QString>;

class Theme : public QObject
{
    Q_OBJECT
public:
    static Theme &instance();

    QStringList availableThemes() const;
    QString currentTheme() const;
    bool isLightTheme(const QString &themeName = QString()) const;
    QString wallpaperResourcePath(const QString &themeName = QString()) const;
    void apply(const QString &themeName);

signals:
    void themeChanged(const QString &name);

private:
    Theme() = default;
    static const QHash<QString, Palette> &palettes();
    static QString generateStylesheet(const Palette &p);
    static void applyQPalette(const Palette &p);
    QString m_current = QStringLiteral("dark");
};

} // namespace flatlas::core
