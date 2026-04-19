#include "UndoManager.h"

namespace flatlas::core {

UndoManager &UndoManager::instance()
{
    static UndoManager mgr;
    return mgr;
}

UndoManager::UndoManager()
{
    connect(&m_stack, &QUndoStack::undoTextChanged, this, &UndoManager::undoTextChanged);
    connect(&m_stack, &QUndoStack::redoTextChanged, this, &UndoManager::redoTextChanged);
    connect(&m_stack, &QUndoStack::canUndoChanged,  this, &UndoManager::canUndoChanged);
    connect(&m_stack, &QUndoStack::canRedoChanged,  this, &UndoManager::canRedoChanged);
    connect(&m_stack, &QUndoStack::cleanChanged,    this, &UndoManager::cleanChanged);
}

QUndoStack *UndoManager::stack() { return &m_stack; }
void UndoManager::push(QUndoCommand *command) { m_stack.push(command); }
void UndoManager::clear() { m_stack.clear(); }

void UndoManager::undo() { m_stack.undo(); }
void UndoManager::redo() { m_stack.redo(); }

bool UndoManager::canUndo() const { return m_stack.canUndo(); }
bool UndoManager::canRedo() const { return m_stack.canRedo(); }
bool UndoManager::isClean() const { return m_stack.isClean(); }
void UndoManager::setClean() { m_stack.setClean(); }
int UndoManager::undoLimit() const { return m_stack.undoLimit(); }
void UndoManager::setUndoLimit(int limit) { m_stack.setUndoLimit(limit); }

} // namespace flatlas::core
