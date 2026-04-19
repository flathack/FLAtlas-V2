#pragma once
#include <QObject>
class QStatusBar;
namespace flatlas::ui {
class StatusBarManager : public QObject {
    Q_OBJECT
public:
    explicit StatusBarManager(QStatusBar *statusBar, QObject *parent = nullptr);
    void showMessage(const QString &message, int timeout = 3000);
private:
    QStatusBar *m_statusBar;
};
} // namespace flatlas::ui
