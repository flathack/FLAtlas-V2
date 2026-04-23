#include "IniMiniMap.h"

#include "IniCodeEditor.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextDocumentLayout>
#include <QScrollBar>
#include <QTextDocument>

namespace flatlas::editors {

namespace {

int boundedValue(int value, int minimum, int maximum)
{
    return qMax(minimum, qMin(value, maximum));
}

}

IniMiniMap::IniMiniMap(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_emptyDocument(new QTextDocument(this))
    , m_previewDocument(new QTextDocument(this))
{
    m_emptyDocument->setDocumentLayout(new QPlainTextDocumentLayout(m_emptyDocument));
    m_previewDocument->setDocumentLayout(new QPlainTextDocumentLayout(m_previewDocument));
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setDocument(m_emptyDocument);
}

void IniMiniMap::setSourceEditor(IniCodeEditor *editor)
{
    QTextDocument *nextDocument = editor ? editor->document() : nullptr;
    if (m_sourceEditor == editor && m_sourceDocument == nextDocument) {
        syncDocumentFromSource();
        syncFromSource();
        return;
    }

    if (m_sourceEditor) {
        disconnect(m_scrollConnection);
        disconnect(m_cursorConnection);
        disconnect(m_updateConnection);
    }
    if (m_sourceDocument)
        disconnect(m_documentConnection);

    m_sourceEditor = editor;
    m_sourceDocument = nextDocument;
    if (!m_sourceEditor) {
        m_sourceDocument = nullptr;
        setDocument(m_emptyDocument);
        verticalScrollBar()->setValue(0);
        viewport()->update();
        return;
    }

    m_previewDocument->setDefaultFont(font());
    setDocument(m_previewDocument);
    syncDocumentFromSource();
    m_scrollConnection = connect(m_sourceEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                                 this, [this]() { syncFromSource(); });
    m_cursorConnection = connect(m_sourceEditor, &QPlainTextEdit::cursorPositionChanged,
                                 this, [this]() { viewport()->update(); });
    m_updateConnection = connect(m_sourceEditor, &QPlainTextEdit::updateRequest,
                                 this, [this](const QRect &, int) { syncFromSource(); });
    m_documentConnection = connect(m_sourceDocument, &QTextDocument::contentsChanged,
                                   this, [this]() { syncDocumentFromSource(); });
    syncFromSource();
}

void IniMiniMap::syncDocumentFromSource()
{
    if (!m_sourceDocument) {
        m_previewDocument->clear();
        return;
    }

    const QString sourceText = m_sourceDocument->toPlainText();
    if (m_previewDocument->toPlainText() == sourceText)
        return;

    QSignalBlocker blocker(this);
    m_previewDocument->setPlainText(sourceText);
}

void IniMiniMap::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);

    if (!m_sourceEditor)
        return;

    QPainter painter(viewport());
    const QRect marker = viewportMarkerRect();
    if (!marker.isValid())
        return;

    QColor fill = palette().color(QPalette::Highlight);
    fill.setAlpha(40);
    QColor border = palette().color(QPalette::Highlight);
    border.setAlpha(150);

    painter.fillRect(marker, fill);
    painter.setPen(QPen(border, 1.0));
    painter.drawRect(marker.adjusted(0, 0, -1, -1));
}

void IniMiniMap::mousePressEvent(QMouseEvent *event)
{
    if (!m_sourceEditor || event->button() != Qt::LeftButton) {
        QPlainTextEdit::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    const QRect marker = viewportMarkerRect();
    m_dragOffset = marker.contains(event->pos()) ? (event->pos().y() - marker.top()) : (marker.height() / 2);
    scrollSourceFromPosition(event->pos().y(), true);
    event->accept();
}

void IniMiniMap::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || !m_sourceEditor) {
        QPlainTextEdit::mouseMoveEvent(event);
        return;
    }

    scrollSourceFromPosition(event->pos().y(), true);
    event->accept();
}

void IniMiniMap::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    QPlainTextEdit::mouseReleaseEvent(event);
}

QRect IniMiniMap::viewportMarkerRect() const
{
    if (!m_sourceEditor)
        return {};

    const int sourceMax = m_sourceEditor->verticalScrollBar()->maximum();
    const int viewHeight = viewport()->height();
    if (viewHeight <= 0)
        return {};

    const int sourceViewportHeight = m_sourceEditor->viewport()->height();
    const int sourceTotalHeight = qMax(1, sourceMax + sourceViewportHeight);
    const double visibleFraction = qBound(0.05,
                                          static_cast<double>(sourceViewportHeight) / static_cast<double>(sourceTotalHeight),
                                          1.0);
    const int markerHeight = qMax(18, static_cast<int>(visibleFraction * static_cast<double>(viewHeight)));
    const int travel = qMax(0, viewHeight - markerHeight);
    const double ratio = sourceMax > 0
        ? static_cast<double>(m_sourceEditor->verticalScrollBar()->value()) / static_cast<double>(sourceMax)
        : 0.0;
    const int y = static_cast<int>(ratio * static_cast<double>(travel));
    return QRect(0, y, viewport()->width(), markerHeight);
}

void IniMiniMap::syncFromSource()
{
    if (!m_sourceEditor) {
        verticalScrollBar()->setValue(0);
        viewport()->update();
        return;
    }

    const int sourceMax = m_sourceEditor->verticalScrollBar()->maximum();
    const int minimapMax = verticalScrollBar()->maximum();
    if (sourceMax <= 0 || minimapMax <= 0) {
        verticalScrollBar()->setValue(0);
        viewport()->update();
        return;
    }

    const double ratio = static_cast<double>(m_sourceEditor->verticalScrollBar()->value()) / static_cast<double>(sourceMax);
    verticalScrollBar()->setValue(static_cast<int>(ratio * static_cast<double>(minimapMax)));
    viewport()->update();
}

void IniMiniMap::scrollSourceFromPosition(int y, bool preserveGrabOffset)
{
    if (!m_sourceEditor)
        return;

    const QRect marker = viewportMarkerRect();
    const int markerHeight = qMax(1, marker.height());
    const int trackHeight = qMax(1, viewport()->height() - markerHeight);
    const int targetTop = boundedValue(y - (preserveGrabOffset ? m_dragOffset : markerHeight / 2),
                                       0,
                                       trackHeight);
    const double ratio = static_cast<double>(targetTop) / static_cast<double>(trackHeight);
    const int sourceMax = m_sourceEditor->verticalScrollBar()->maximum();
    m_sourceEditor->verticalScrollBar()->setValue(static_cast<int>(ratio * static_cast<double>(sourceMax)));
}

} // namespace flatlas::editors