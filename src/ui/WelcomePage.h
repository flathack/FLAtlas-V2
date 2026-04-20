#pragma once
#include <QWidget>
class QListWidget;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
namespace flatlas::ui {
class WelcomePage : public QWidget {
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);
signals:
    void openModManagerRequested();
private:
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QCheckBox *m_updateCheck = nullptr;
    QLabel *m_idsStatusLabel = nullptr;
};
} // namespace flatlas::ui
