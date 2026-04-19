#include "I18n.h"

namespace flatlas::core {

I18n &I18n::instance()
{
    static I18n i18n;
    return i18n;
}

QStringList I18n::availableLanguages()
{
    return {QStringLiteral("de"), QStringLiteral("en")};
}

void I18n::removeTranslators()
{
    if (m_appTranslator) {
        QCoreApplication::removeTranslator(m_appTranslator);
        delete m_appTranslator;
        m_appTranslator = nullptr;
    }
    if (m_qtTranslator) {
        QCoreApplication::removeTranslator(m_qtTranslator);
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }
}

void I18n::setLanguage(const QString &langCode)
{
    if (m_language == langCode)
        return;

    removeTranslators();

    m_appTranslator = new QTranslator(this);
    if (m_appTranslator->load(QStringLiteral(":/translations/flatlas_%1.qm").arg(langCode)))
        QCoreApplication::installTranslator(m_appTranslator);

    m_qtTranslator = new QTranslator(this);
    if (m_qtTranslator->load(QStringLiteral(":/translations/qt_%1.qm").arg(langCode)))
        QCoreApplication::installTranslator(m_qtTranslator);

    m_language = langCode;
    emit languageChanged();
}

QString I18n::currentLanguage() const { return m_language; }

} // namespace flatlas::core
