#pragma once
// core/UndoManager.h – QUndoStack-basierter Undo/Redo-Manager

#include <QObject>
#include <QUndoStack>

namespace flatlas::core {

class UndoManager : public QObject
{
    Q_OBJECT
public:
    static UndoManager &instance();

    QUndoStack *stack();
    void push(QUndoCommand *command);
    void clear();

    bool canUndo() const;
    bool canRedo() const;
    bool isClean() const;
    void setClean();
    int undoLimit() const;
    void setUndoLimit(int limit);

public slots:
    void undo();
    void redo();

signals:
    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void cleanChanged(bool clean);

private:
    UndoManager();
    QUndoStack m_stack;
};

} // namespace flatlas::core
