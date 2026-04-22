#pragma once
// editors/ini/IniEditorPage.h – INI-Editor mit Syntax-Highlighting (Phase 6)

#include <QWidget>

class QToolBar;
class QLineEdit;
class QCheckBox;
class QLabel;

namespace flatlas::editors {

class IniCodeEditor;
class IniSyntaxHighlighter;

class IniEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit IniEditorPage(QWidget *parent = nullptr);

    bool openFile(const QString &filePath);
    bool save();
    bool saveAs(const QString &filePath);

    QString filePath() const;
    QString fileName() const;
    bool isDirty() const;
    void focusSearch(const QString &text);

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupSearchBar();
    void updateTitle();
    void onTextChanged();
    void findNext();
    void findPrev();
    void replaceAll();

    IniCodeEditor *m_editor = nullptr;
    IniSyntaxHighlighter *m_highlighter = nullptr;
    QToolBar *m_searchBar = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QCheckBox *m_caseSensitiveCheck = nullptr;
    QLabel *m_statusLabel = nullptr;

    QString m_filePath;
    bool m_dirty = false;
    bool m_wasBini = false;
};

} // namespace flatlas::editors
