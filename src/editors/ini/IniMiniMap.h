#pragma once

#include <QPlainTextEdit>
#include <QPointer>

class QTextDocument;

namespace flatlas::editors {

class IniCodeEditor;

class IniMiniMap : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit IniMiniMap(QWidget *parent = nullptr);

    void setSourceEditor(IniCodeEditor *editor);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect viewportMarkerRect() const;
    void syncDocumentFromSource();
    void syncFromSource();
    void scrollSourceFromPosition(int y, bool preserveGrabOffset);

    QPointer<IniCodeEditor> m_sourceEditor;
    QPointer<QTextDocument> m_sourceDocument;
    QTextDocument *m_emptyDocument = nullptr;
    QTextDocument *m_previewDocument = nullptr;
    bool m_dragging = false;
    int m_dragOffset = 0;
    QMetaObject::Connection m_scrollConnection;
    QMetaObject::Connection m_cursorConnection;
    QMetaObject::Connection m_updateConnection;
    QMetaObject::Connection m_documentConnection;
};

} // namespace flatlas::editors