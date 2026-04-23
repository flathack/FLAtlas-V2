#include "IniCodeEditor.h"

#include <QFontDatabase>
#include <QPainter>
#include <QTextBlock>

namespace flatlas::editors {

// --- LineNumberArea ---

LineNumberArea::LineNumberArea(IniCodeEditor *editor)
    : QWidget(editor), m_codeEditor(editor) {}

QSize LineNumberArea::sizeHint() const {
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event) {
    m_codeEditor->lineNumberAreaPaintEvent(event);
}

// --- IniCodeEditor ---

IniCodeEditor::IniCodeEditor(QWidget *parent)
    : QPlainTextEdit(parent) {
    m_lineNumberArea = new LineNumberArea(this);

    QFont font("Consolas", 10);
    if (!QFontDatabase::hasFamily("Consolas"))
        font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    setFont(font);

    const int tabWidth = 4;
    setTabStopDistance(QFontMetricsF(font).horizontalAdvance(' ') * tabWidth);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &IniCodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &IniCodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &IniCodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int IniCodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void IniCodeEditor::updateLineNumberAreaWidth(int /*newBlockCount*/) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void IniCodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void IniCodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void IniCodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), palette().midlight());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    int currentBlockNumber = textCursor().blockNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            if (blockNumber == currentBlockNumber) {
                QFont boldFont = painter.font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
                painter.setPen(palette().color(QPalette::Text));
            } else {
                QFont normalFont = painter.font();
                normalFont.setBold(false);
                painter.setFont(normalFont);
                QColor dimmed = palette().color(QPalette::Text);
                dimmed.setAlphaF(0.5f);
                painter.setPen(dimmed);
            }

            painter.drawText(0, top, m_lineNumberArea->width() - 5,
                             fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void IniCodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(46, 62, 84, 110));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

bool IniCodeEditor::findText(const QString &text, bool caseSensitive,
                             bool wholeWord, bool forward) {
    QTextDocument::FindFlags flags;
    if (!forward)
        flags |= QTextDocument::FindBackward;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;
    if (wholeWord)
        flags |= QTextDocument::FindWholeWords;

    bool found = find(text, flags);
    if (!found) {
        // Wrap around
        QTextCursor cursor = textCursor();
        if (forward) {
            cursor.movePosition(QTextCursor::Start);
        } else {
            cursor.movePosition(QTextCursor::End);
        }
        setTextCursor(cursor);
        found = find(text, flags);
    }
    return found;
}

int IniCodeEditor::replaceAll(const QString &searchText,
                              const QString &replaceText, bool caseSensitive) {
    int count = 0;

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    setTextCursor(cursor);

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;

    while (find(searchText, flags)) {
        QTextCursor found = textCursor();
        found.insertText(replaceText);
        ++count;
    }

    cursor.endEditBlock();
    return count;
}

void IniCodeEditor::goToLine(int lineNumber) {
    QTextBlock block = document()->findBlockByNumber(lineNumber - 1);
    if (block.isValid()) {
        QTextCursor cursor(block);
        setTextCursor(cursor);
        ensureCursorVisible();
    }
}

}
