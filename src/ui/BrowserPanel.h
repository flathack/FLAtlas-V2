#pragma once
#include <QWidget>
class QTreeView;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItemModel;
namespace flatlas::ui {
class BrowserPanel : public QWidget {
    Q_OBJECT
public:
    explicit BrowserPanel(QWidget *parent = nullptr);
    void loadSystems(const QStringList &systemNames);
signals:
    void systemSelected(const QString &systemFile);
private:
    QLineEdit *m_filterEdit = nullptr;
    QTreeView *m_treeView = nullptr;
    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;
};
} // namespace flatlas::ui
