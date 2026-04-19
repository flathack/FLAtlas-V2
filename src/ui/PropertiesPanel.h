#pragma once
#include <QWidget>
class QTreeView;
namespace flatlas::ui {
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget *parent = nullptr);
    void showProperties(const QVariantMap &properties);
    void clear();
private:
    QTreeView *m_treeView = nullptr;
};
} // namespace flatlas::ui
