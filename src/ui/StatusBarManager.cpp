#include "StatusBarManager.h"
#include <QStatusBar>
namespace flatlas::ui {
StatusBarManager::StatusBarManager(QStatusBar *statusBar, QObject *parent)
    : QObject(parent), m_statusBar(statusBar) {}
void StatusBarManager::showMessage(const QString &message, int timeout)
{
    m_statusBar->showMessage(message, timeout);
}
} // namespace flatlas::ui
