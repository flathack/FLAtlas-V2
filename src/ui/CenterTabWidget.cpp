#include "CenterTabWidget.h"
#include <QTabBar>
#include <QStackedWidget>

namespace flatlas::ui {

CenterTabWidget::CenterTabWidget(QObject *parent)
    : QObject(parent)
    , m_tabBar(new QTabBar)
    , m_stack(new QStackedWidget)
{
    m_tabBar->setExpanding(false);
    m_tabBar->setMovable(false);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setElideMode(Qt::ElideRight);

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0 && index < m_stack->count())
            m_stack->setCurrentIndex(index);
        emit currentChanged(index);
    });

    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (index < m_pinnedCount)
            return;
        removeTab(index);
    });
}

QTabBar *CenterTabWidget::tabBar() const { return m_tabBar; }
QStackedWidget *CenterTabWidget::contentWidget() const { return m_stack; }

int CenterTabWidget::addTab(QWidget *widget, const QString &label)
{
    int idx = m_tabBar->addTab(label);
    m_stack->addWidget(widget);
    return idx;
}

int CenterTabWidget::addPinnedTab(QWidget *widget, const QString &label)
{
    int idx = m_pinnedCount;
    m_tabBar->insertTab(idx, label);
    m_stack->insertWidget(idx, widget);
    m_tabBar->setTabButton(idx, QTabBar::RightSide, nullptr);
    m_tabBar->setTabButton(idx, QTabBar::LeftSide, nullptr);
    ++m_pinnedCount;
    return idx;
}

void CenterTabWidget::removeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count() || index < m_pinnedCount)
        return;
    QWidget *w = m_stack->widget(index);
    m_tabBar->removeTab(index);
    m_stack->removeWidget(w);
    w->deleteLater();
}

void CenterTabWidget::setCurrentIndex(int index)
{
    m_tabBar->setCurrentIndex(index);
}

int CenterTabWidget::currentIndex() const { return m_tabBar->currentIndex(); }
int CenterTabWidget::count() const { return m_tabBar->count(); }
QWidget *CenterTabWidget::currentWidget() const { return m_stack->currentWidget(); }
QWidget *CenterTabWidget::widget(int index) const { return m_stack->widget(index); }
int CenterTabWidget::indexOf(QWidget *widget) const { return m_stack->indexOf(widget); }

void CenterTabWidget::setTabText(int index, const QString &text)
{
    m_tabBar->setTabText(index, text);
}

} // namespace flatlas::ui
