#pragma once
// tools/HelpBrowser.h – In-App-Hilfe-System

#include <QDialog>
#include <QMap>
#include <QString>

class QTextBrowser;
class QListWidget;
class QSplitter;

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

    /// Lädt eingebaute Standard-Hilfe-Themen.
    void loadBuiltinTopics();

    /// Gibt die registrierten Topic-IDs zurück.
    QStringList topicIds() const;

    /// Mapping von Editor-Klasse zu Hilfe-Topic-ID für F1-Kontext-Hilfe.
    static QString topicForContext(const QString &contextId);

private:
    void buildUi();

    QSplitter *m_splitter = nullptr;
    QListWidget *m_topicList = nullptr;
    QTextBrowser *m_browser = nullptr;
    QMap<QString, HelpTopic> m_topics;
};

} // namespace flatlas::tools
