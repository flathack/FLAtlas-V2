#pragma once
#include <QWidget>
namespace flatlas::ui {
class WelcomePage : public QWidget {
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);
signals:
    void openFileRequested(const QString &path);
};
} // namespace flatlas::ui
