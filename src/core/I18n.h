#pragma once
// core/I18n.h – Übersetzungsmanagement (Qt Linguist)

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QCoreApplication>

namespace flatlas::core {

class I18n : public QObject
{
    Q_OBJECT
public:
    static I18n &instance();
    static QStringList availableLanguages();
    void setLanguage(const QString &langCode);
    QString currentLanguage() const;

signals:
    void languageChanged();

private:
    I18n() = default;
    void removeTranslators();
    QString m_language = QStringLiteral("de");
    QTranslator *m_appTranslator = nullptr;
    QTranslator *m_qtTranslator = nullptr;
};

} // namespace flatlas::core
