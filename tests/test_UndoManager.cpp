#include <QtTest>
#include <QUndoCommand>
#include "core/UndoManager.h"

using flatlas::core::UndoManager;

// Simple test command that toggles a bool
class ToggleCommand : public QUndoCommand {
public:
    explicit ToggleCommand(bool &target)
        : QUndoCommand(QStringLiteral("Toggle")), m_target(target) {}
    void redo() override { m_target = !m_target; }
    void undo() override { m_target = !m_target; }
private:
    bool &m_target;
};

class TestUndoManager : public QObject {
    Q_OBJECT
private slots:
    void singletonIdentity()
    {
        auto &a = UndoManager::instance();
        auto &b = UndoManager::instance();
        QCOMPARE(&a, &b);
    }

    void stackNotNull()
    {
        QVERIFY(UndoManager::instance().stack() != nullptr);
    }

    void pushAndUndo()
    {
        auto &mgr = UndoManager::instance();
        mgr.clear();

        bool flag = false;
        mgr.push(new ToggleCommand(flag));
        QVERIFY(flag);            // redo was called
        QVERIFY(mgr.canUndo());

        mgr.undo();
        QVERIFY(!flag);           // undo was called
        QVERIFY(!mgr.canUndo());
        QVERIFY(mgr.canRedo());
    }

    void pushAndRedo()
    {
        auto &mgr = UndoManager::instance();
        mgr.clear();

        bool flag = false;
        mgr.push(new ToggleCommand(flag));
        mgr.undo();
        QVERIFY(!flag);

        mgr.redo();
        QVERIFY(flag);
        QVERIFY(!mgr.canRedo());
    }

    void clearResetsState()
    {
        auto &mgr = UndoManager::instance();
        bool flag = false;
        mgr.push(new ToggleCommand(flag));
        QVERIFY(mgr.canUndo());

        mgr.clear();
        QVERIFY(!mgr.canUndo());
        QVERIFY(!mgr.canRedo());
    }

    void isCleanAfterSetClean()
    {
        auto &mgr = UndoManager::instance();
        mgr.clear();

        bool flag = false;
        mgr.push(new ToggleCommand(flag));
        QVERIFY(!mgr.isClean());

        mgr.setClean();
        QVERIFY(mgr.isClean());
    }
};

QTEST_MAIN(TestUndoManager)
#include "test_UndoManager.moc"
