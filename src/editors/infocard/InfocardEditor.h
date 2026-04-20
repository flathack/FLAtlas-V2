#pragma once
// editors/infocard/InfocardEditor.h – Rich-text preview + XML source editor

#include <QWidget>

class QTextBrowser;
class QPlainTextEdit;
class QTabWidget;
class QLabel;
class QPushButton;

namespace flatlas::editors {

class InfocardEditor : public QWidget {
    Q_OBJECT
public:
    explicit InfocardEditor(QWidget *parent = nullptr);

    /// Load infocard XML source into the editor.
    void loadInfocard(const QString &xml);

    /// Load infocard from a file (reads raw content).
    void loadFromFile(const QString &filePath);

    /// Return current XML source.
    QString xmlSource() const;

    /// Return true if content was modified since last load/save.
    bool isModified() const { return m_modified; }

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void refreshPreview();
    void validateAndReport();
    void onSourceChanged();

    QTabWidget      *m_tabs         = nullptr;
    QTextBrowser    *m_preview      = nullptr;
    QPlainTextEdit  *m_sourceEdit   = nullptr;
    QLabel          *m_statusLabel  = nullptr;
    QPushButton     *m_validateBtn  = nullptr;
    QPushButton     *m_wrapBtn      = nullptr;

    bool m_modified = false;
};

} // namespace flatlas::editors
