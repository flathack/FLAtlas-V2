#pragma once
// core/I18n.h – Übersetzungsmanagement (Qt Linguist)
// TODO Phase 1

#include <QObject>
#include <QString>

namespace flatlas::core {

class I18n : public QObject
{
    Q_OBJECT
public:
    static I18n &instance();
    void setLanguage(const QString &langCode);
    QString currentLanguage() const;

signals:
    void languageChanged();

private:
    I18n() = default;
    QString m_language = QStringLiteral("de");
};

} // namespace flatlas::core
