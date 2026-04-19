#include "Theme.h"
// TODO Phase 1

namespace flatlas::core {

Theme &Theme::instance()
{
    static Theme theme;
    return theme;
}

QStringList Theme::availableThemes() const
{
    return {QStringLiteral("dark"), QStringLiteral("light"),
            QStringLiteral("founder"), QStringLiteral("xp")};
}

QString Theme::currentTheme() const { return m_current; }

void Theme::apply(const QString &themeName)
{
    m_current = themeName;
    // TODO: QSS laden und qApp->setStyleSheet() aufrufen
    emit themeChanged(themeName);
}

} // namespace flatlas::core
