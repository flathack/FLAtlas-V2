#include "CenterTabWidget.h"
#include "ToolIcons.h"
#include <QIcon>
#include <QSize>
#include <QTabBar>
#include <QStackedWidget>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

namespace flatlas::ui {

CenterTabWidget::CenterTabWidget(QObject *parent)
    : QObject(parent)
    , m_tabBar(new QTabBar)
    , m_stack(new QStackedWidget)
{
    m_tabBar->setExpanding(false);
    m_tabBar->setMovable(true);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setIconSize(QSize(28, 28));

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (QWidget *page = widgetForTab(index))
            m_stack->setCurrentWidget(page);
        emit currentChanged(index);
    });

    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (isPinnedTab(index))
            return;
        emit closeRequested(index);
    });

    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        Q_UNUSED(from);
        if (QWidget *page = widgetForTab(to))
            m_stack->setCurrentWidget(page);
    });
}

QTabBar *CenterTabWidget::tabBar() const { return m_tabBar; }
QStackedWidget *CenterTabWidget::contentWidget() const { return m_stack; }

int CenterTabWidget::addTab(QWidget *widget, const QString &label)
{
    return addTab(widget, QIcon(), label);
}

int CenterTabWidget::addTab(QWidget *widget, const QIcon &icon, const QString &label)
{
    int idx = m_tabBar->addTab(label);
    if (!icon.isNull())
        m_tabBar->setTabIcon(idx, icon);
    m_tabBar->setTabData(idx, QVariant::fromValue(static_cast<QObject *>(widget)));
    m_tabBar->setTabButton(idx, QTabBar::RightSide, createCloseButton(widget));
    m_stack->addWidget(widget);
    return idx;
}

int CenterTabWidget::addPinnedTab(QWidget *widget, const QString &label)
{
    return addPinnedTab(widget, QIcon(), label);
}

int CenterTabWidget::addPinnedTab(QWidget *widget, const QIcon &icon, const QString &label)
{
    int idx = m_pinnedCount;
    m_tabBar->insertTab(idx, label);
    if (!icon.isNull())
        m_tabBar->setTabIcon(idx, icon);
    m_tabBar->setTabData(idx, QVariant::fromValue(static_cast<QObject *>(widget)));
    m_stack->addWidget(widget);
    m_pinnedWidgets.insert(widget);
    m_tabBar->setTabButton(idx, QTabBar::RightSide, nullptr);
    m_tabBar->setTabButton(idx, QTabBar::LeftSide, nullptr);
    ++m_pinnedCount;
    return idx;
}

void CenterTabWidget::removeTab(int index, bool force)
{
    if (index < 0 || index >= m_tabBar->count() || (!force && isPinnedTab(index)))
        return;
    QWidget *w = widgetForTab(index);
    if (force && isPinnedTab(index))
        --m_pinnedCount;
    if (w)
        m_pinnedWidgets.remove(w);
    if (w)
        m_stack->removeWidget(w);
    m_tabBar->removeTab(index);
    if (w)
        w->deleteLater();
}

void CenterTabWidget::setCurrentIndex(int index)
{
    if (QWidget *page = widgetForTab(index))
        m_stack->setCurrentWidget(page);
    m_tabBar->setCurrentIndex(index);
}

int CenterTabWidget::currentIndex() const { return m_tabBar->currentIndex(); }
int CenterTabWidget::count() const { return m_tabBar->count(); }
QWidget *CenterTabWidget::currentWidget() const { return widgetForTab(m_tabBar->currentIndex()); }
QWidget *CenterTabWidget::widget(int index) const { return widgetForTab(index); }

int CenterTabWidget::indexOf(QWidget *widget) const
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (widgetForTab(i) == widget)
            return i;
    }
    return -1;
}

void CenterTabWidget::setTabText(int index, const QString &text)
{
    m_tabBar->setTabText(index, text);
}

bool CenterTabWidget::isPinnedTab(int index) const
{
    QWidget *widget = widgetForTab(index);
    return widget && m_pinnedWidgets.contains(widget);
}

QWidget *CenterTabWidget::createCloseButton(QWidget *tabWidget)
{
    auto *button = new QToolButton(m_tabBar);
    button->setObjectName(QStringLiteral("centerTabCloseButton"));
    button->setAutoRaise(true);
    button->setCursor(Qt::ArrowCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setIcon(closeIcon());
    button->setIconSize(QSize(14, 14));
    button->setFixedSize(22, 22);
    button->setToolTip(tr("Tab schließen"));
    connect(button, &QToolButton::clicked, this, [this, tabWidget]() {
        const int index = indexOf(tabWidget);
        if (index >= 0 && !isPinnedTab(index))
            emit closeRequested(index);
    });
    return button;
}

QWidget *CenterTabWidget::widgetForTab(int index) const
{
    if (index < 0 || index >= m_tabBar->count())
        return nullptr;
    QObject *object = m_tabBar->tabData(index).value<QObject *>();
    return qobject_cast<QWidget *>(object);
}

} // namespace flatlas::ui
