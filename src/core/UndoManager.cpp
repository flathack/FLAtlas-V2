#include "UndoManager.h"

namespace flatlas::core {

UndoManager &UndoManager::instance()
{
    static UndoManager mgr;
    return mgr;
}

QUndoStack *UndoManager::stack() { return &m_stack; }
void UndoManager::push(QUndoCommand *command) { m_stack.push(command); }
void UndoManager::clear() { m_stack.clear(); }

} // namespace flatlas::core
