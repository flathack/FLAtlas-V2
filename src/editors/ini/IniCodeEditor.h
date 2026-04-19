#pragma once
#include <QPlainTextEdit>

class QPaintEvent;
class QResizeEvent;

namespace flatlas::editors {

class LineNumberArea;

class IniCodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit IniCodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;

    void goToLine(int lineNumber);

    // Search
    bool findText(const QString &text, bool caseSensitive = false, bool wholeWord = false, bool forward = true);
    int replaceAll(const QString &searchText, const QString &replaceText, bool caseSensitive = false);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

private:
    LineNumberArea *m_lineNumberArea;
};

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(IniCodeEditor *editor);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    IniCodeEditor *m_codeEditor;
};

}
