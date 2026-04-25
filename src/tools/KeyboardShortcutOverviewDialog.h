#pragma once

#include <QDialog>

class QTableWidget;
class QLabel;

namespace flatlas::tools {

class KeyboardShortcutOverviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyboardShortcutOverviewDialog(QWidget *parent = nullptr);

private:
    struct ShortcutRow {
        QString section;
        QString shortcut;
        QString context;
        QString description;
    };

    void buildUi();
    void populateRows();
    void addSectionHeaderRow(const QString &title, int row);
    void addShortcutRow(const ShortcutRow &row, int rowIndex);

    QLabel *m_introLabel = nullptr;
    QTableWidget *m_table = nullptr;
};

} // namespace flatlas::tools