#pragma once
#include <QWidget>
class QListWidget;
namespace flatlas::ui {
class WelcomePage : public QWidget {
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);
    void updateRecentFiles();
signals:
    void openFileRequested(const QString &path);
private:
    QListWidget *m_recentList = nullptr;
};
} // namespace flatlas::ui
