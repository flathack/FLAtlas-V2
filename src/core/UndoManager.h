#pragma once
// core/UndoManager.h – QUndoStack-basierter Undo/Redo-Manager
// TODO Phase 1

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

private:
    UndoManager() = default;
    QUndoStack m_stack;
};

} // namespace flatlas::core
