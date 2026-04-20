#pragma once
#include <QObject>

class QTabBar;
class QStackedWidget;

namespace flatlas::ui {

/// Tab controller that manages a separate QTabBar and QStackedWidget,
/// allowing the tab bar to be placed independently from the content area.
class CenterTabWidget : public QObject {
    Q_OBJECT
public:
    explicit CenterTabWidget(QObject *parent = nullptr);

    /// The tab bar widget – place this wherever the tabs should appear.
    QTabBar *tabBar() const;
    /// The stacked content widget – place this where pages should display.
    QStackedWidget *contentWidget() const;

    int addTab(QWidget *widget, const QString &label);
    int addPinnedTab(QWidget *widget, const QString &label);
    void removeTab(int index);
    void setCurrentIndex(int index);
    int currentIndex() const;
    int count() const;
    QWidget *currentWidget() const;
    QWidget *widget(int index) const;
    int indexOf(QWidget *widget) const;
    void setTabText(int index, const QString &text);

signals:
    void currentChanged(int index);

private:
    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    int m_pinnedCount = 0;
};

} // namespace flatlas::ui
