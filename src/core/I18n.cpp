#include "I18n.h"
// TODO Phase 1

namespace flatlas::core {

I18n &I18n::instance()
{
    static I18n i18n;
    return i18n;
}

void I18n::setLanguage(const QString &langCode)
{
    if (m_language != langCode) {
        m_language = langCode;
        emit languageChanged();
    }
}

QString I18n::currentLanguage() const { return m_language; }

} // namespace flatlas::core
