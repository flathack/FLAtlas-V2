#pragma once
// tools/HelpBrowser.h – In-App-Hilfe-System

#include <QDialog>
#include <QMap>
#include <QString>
#include <QVector>

class QTextBrowser;
class QListWidget;
class QSplitter;

namespace flatlas::domain::guide {
struct GuideArticle;
}

namespace flatlas::infrastructure::guide {
class GuideRepository;
}

namespace flatlas::tools {

/// Hilfe-Thema mit Titel und HTML-Inhalt.
struct HelpTopic {
    QString id;
    QString title;
    QString html;
};

/// In-App-Hilfe mit kontextsensitiver Navigation.
class HelpBrowser : public QDialog
{
    Q_OBJECT

public:
    explicit HelpBrowser(QWidget *parent = nullptr);

    /// Thema anzeigen (per ID).
    void showTopic(const QString &topicId);

    /// Registriert ein Hilfe-Thema.
    void registerTopic(const HelpTopic &topic);

    void showEmptyState();

    /// Lädt eingebaute Standard-Hilfe-Themen.
    void loadBuiltinTopics();

    /// Lädt Guide-Artikel und ersetzt die aktuell registrierten Themen.
    bool loadGuideArticles(const QVector<flatlas::domain::guide::GuideArticle> &articles);

    /// Lädt alle Artikel aus dem aktiven Guide-Repository.
    bool loadGuideRepository(const flatlas::infrastructure::guide::GuideRepository &repository,
                             const QString &language = QString(),
                             QString *errorMessage = nullptr);

    /// Gibt die registrierten Topic-IDs zurück.
    QStringList topicIds() const;

    /// Mapping von Editor-Klasse zu Hilfe-Topic-ID für F1-Kontext-Hilfe.
    static QString topicForContext(const QString &contextId);

private:
    void buildUi();
    void clearTopics();
    QString resolveTopicId(const QString &topicId) const;

    QSplitter *m_splitter = nullptr;
    QListWidget *m_topicList = nullptr;
    QTextBrowser *m_browser = nullptr;
    QMap<QString, HelpTopic> m_topics;
    QMap<QString, QString> m_topicAliases;
};

} // namespace flatlas::tools
