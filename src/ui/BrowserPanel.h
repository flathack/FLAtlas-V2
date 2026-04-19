#pragma once
#include <QWidget>
class QTreeView;
class QLineEdit;
namespace flatlas::ui {
class BrowserPanel : public QWidget {
    Q_OBJECT
public:
    explicit BrowserPanel(QWidget *parent = nullptr);
signals:
    void systemSelected(const QString &systemFile);
private:
    QLineEdit *m_filterEdit = nullptr;
    QTreeView *m_treeView = nullptr;
};
} // namespace flatlas::ui
