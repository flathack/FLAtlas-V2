#pragma once
#include <QWidget>
#include <QVector>
#include "domain/UniverseData.h"
class QTreeView;
class QLineEdit;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
namespace flatlas::ui {
class BrowserPanel : public QWidget {
    Q_OBJECT
public:
    explicit BrowserPanel(QWidget *parent = nullptr);
    void loadSystems(const QStringList &systemNames);
    void refreshFromContext();
signals:
    void systemSelected(const QString &nickname, const QString &systemFile);
private:
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QLabel *m_systemsLabel = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QTreeView *m_treeView = nullptr;
    QLabel *m_statusLabel = nullptr;
    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;
    QString m_dataDir;
    QVector<flatlas::domain::SystemInfo> m_systems;
};
} // namespace flatlas::ui
