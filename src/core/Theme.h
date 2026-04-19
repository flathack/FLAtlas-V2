#pragma once
// core/Theme.h – QSS-Paletten und dynamischer Theme-Wechsel
// TODO Phase 1

#include <QObject>
#include <QString>
#include <QStringList>

namespace flatlas::core {

class Theme : public QObject
{
    Q_OBJECT
public:
    static Theme &instance();

    QStringList availableThemes() const;
    QString currentTheme() const;
    void apply(const QString &themeName);

signals:
    void themeChanged(const QString &name);

private:
    Theme() = default;
    QString m_current = QStringLiteral("dark");
};

} // namespace flatlas::core
