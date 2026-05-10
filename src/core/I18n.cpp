#include "I18n.h"

#include "Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QStandardPaths>
#include <utility>

static void initI18nResources()
{
    Q_INIT_RESOURCE(resources);
}

namespace flatlas::core {
namespace {

class RuntimeTranslator final : public QTranslator
{
public:
    explicit RuntimeTranslator(QHash<QString, QString> translations, QObject *parent = nullptr)
        : QTranslator(parent)
        , m_translations(std::move(translations))
    {
    }

    QString translate(const char *, const char *sourceText, const char *, int) const override
    {
        if (!sourceText)
            return {};
        return m_translations.value(QString::fromUtf8(sourceText));
    }

private:
    QHash<QString, QString> m_translations;
};

QString normalizeLanguage(QString langCode)
{
    langCode = langCode.trimmed().toLower();
    const int separator = langCode.indexOf(QLatin1Char('-'));
    if (separator > 0)
        langCode = langCode.left(separator);
    const int underscore = langCode.indexOf(QLatin1Char('_'));
    if (underscore > 0)
        langCode = langCode.left(underscore);
    static const QRegularExpression validCode(QStringLiteral("^[a-z][a-z0-9]{1,7}$"));
    if (!validCode.match(langCode).hasMatch())
        return QStringLiteral("en");
    return langCode;
}

QStringList languageRoots()
{
    initI18nResources();

    QStringList roots;
    roots << QCoreApplication::applicationDirPath() + QStringLiteral("/languages");

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty())
        roots << appData + QStringLiteral("/languages");

    roots << QStringLiteral(":/languages");
    return roots;
}

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

QString customLanguageFilePath()
{
    const QString path = Config::instance().getString(QStringLiteral("customLanguageFile")).trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return {};
    return path;
}

QString customLanguageCode()
{
    const QJsonObject object = readJsonObject(customLanguageFilePath());
    return normalizeLanguage(object.value(QStringLiteral("code")).toString());
}

QHash<QString, QString> loadTranslations(const QString &language)
{
    if (language == QStringLiteral("en"))
        return {};

    const QString customPath = customLanguageFilePath();
    if (!customPath.isEmpty()) {
        const QJsonObject object = readJsonObject(customPath);
        if (normalizeLanguage(object.value(QStringLiteral("code")).toString()) == language) {
            const QJsonObject translationObject = object.value(QStringLiteral("translations")).toObject();
            QHash<QString, QString> translations;
            for (auto it = translationObject.constBegin(); it != translationObject.constEnd(); ++it) {
                const QString value = it.value().toString();
                if (!it.key().isEmpty() && !value.isEmpty())
                    translations.insert(it.key(), value);
            }
            if (!translations.isEmpty())
                return translations;
        }
    }

    const QString fileName = language + QStringLiteral(".json");
    for (const QString &root : languageRoots()) {
        const QJsonObject object = readJsonObject(root + QLatin1Char('/') + fileName);
        if (object.value(QStringLiteral("code")).toString().toLower() != language)
            continue;

        const QJsonObject translationObject = object.value(QStringLiteral("translations")).toObject();
        QHash<QString, QString> translations;
        for (auto it = translationObject.constBegin(); it != translationObject.constEnd(); ++it) {
            const QString value = it.value().toString();
            if (!it.key().isEmpty() && !value.isEmpty())
                translations.insert(it.key(), value);
        }
        if (!translations.isEmpty())
            return translations;
    }

    return {};
}

QStringList loadCatalogLanguages()
{
    QStringList languages{QStringLiteral("en")};
    for (const QString &root : languageRoots()) {
        const QJsonObject object = readJsonObject(root + QStringLiteral("/index.json"));
        const QJsonArray languageArray = object.value(QStringLiteral("languages")).toArray();
        for (const QJsonValue &value : languageArray) {
            const QString code = normalizeLanguage(value.toObject().value(QStringLiteral("code")).toString());
            if (code != QStringLiteral("en") && !languages.contains(code))
                languages << code;
        }
    }
    const QString customCode = customLanguageCode();
    if (customCode != QStringLiteral("en") && !languages.contains(customCode))
        languages << customCode;
    return languages;
}

} // namespace

I18n &I18n::instance()
{
    static I18n i18n;
    return i18n;
}

QStringList I18n::availableLanguages()
{
    return loadCatalogLanguages();
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
    const QString normalized = normalizeLanguage(langCode);
    const QString customPath = customLanguageFilePath();
    if (m_language == normalized && m_customLanguageFile == customPath
        && (normalized == QStringLiteral("en") || m_appTranslator)) {
        return;
    }

    removeTranslators();

    const auto translations = loadTranslations(normalized);
    if (!translations.isEmpty()) {
        m_appTranslator = new RuntimeTranslator(translations, this);
        QCoreApplication::installTranslator(m_appTranslator);
    }

    if (normalized != QStringLiteral("en")) {
        m_qtTranslator = new QTranslator(this);
        const QString qtResource = QStringLiteral(":/translations/qt_%1.qm").arg(normalized);
        const QString qtSystem = QStringLiteral("qt_%1").arg(normalized);
        if (m_qtTranslator->load(qtResource)
            || m_qtTranslator->load(QLocale(normalized), qtSystem, QStringLiteral("_"))) {
            QCoreApplication::installTranslator(m_qtTranslator);
        } else {
            delete m_qtTranslator;
            m_qtTranslator = nullptr;
        }
    }

    m_language = normalized;
    m_customLanguageFile = customPath;
    emit languageChanged();
}

QString I18n::currentLanguage() const
{
    return m_language.isEmpty() ? QStringLiteral("en") : m_language;
}

} // namespace flatlas::core
