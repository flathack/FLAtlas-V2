#include "KeyboardShortcutOverviewDialog.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace flatlas::tools {

namespace {

QTableWidgetItem *makeReadOnlyItem(const QString &text, const QFont &font = QFont())
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~(Qt::ItemIsEditable | Qt::ItemIsSelectable));
    if (!font.family().isEmpty())
        item->setFont(font);
    return item;
}

}

KeyboardShortcutOverviewDialog::KeyboardShortcutOverviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    setWindowFlag(Qt::Window, true);
    setWindowModality(Qt::NonModal);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumSize(900, 560);
    resize(1040, 680);

    buildUi();
    populateRows();
}

void KeyboardShortcutOverviewDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);

    m_introLabel = new QLabel(
        tr("This overview remains open as a separate window and can be moved to a second monitor. "
           "The 2D system editor shortcuts only apply while the System Editor is active. Canvas actions such as tool switching, "
           "moving, rotating, focusing, or canceling additionally require focus in the 2D canvas and do not fire while typing text."),
        this);
    m_introLabel->setWordWrap(true);
    layout->addWidget(m_introLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Shortcut"), tr("Context"), tr("Action")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);
}

void KeyboardShortcutOverviewDialog::populateRows()
{
    const QList<ShortcutRow> rows = {
        {tr("General"), tr("Ctrl+S"), tr("Active Editor"), tr("Saves the current file or editor state.")},
        {tr("General"), tr("Ctrl+Z"), tr("Active Editor"), tr("Undoes the last undoable action.")},
        {tr("General"), tr("Ctrl+Y"), tr("Active Editor"), tr("Redoes the last undone action.")},
        {tr("General"), tr("Ctrl+Shift+Z"), tr("Active Editor"), tr("Alternative redo shortcut.")},
        {tr("General"), tr("F1"), tr("Entire Application"), tr("Opens the context-sensitive help for the current editor or dialog.")},

        {tr("2D System Editor"), tr("Q or V"), tr("2D Canvas Focused"), tr("Switches to the selection tool.")},
        {tr("2D System Editor"), tr("W"), tr("2D Canvas Focused"), tr("Switches to the move tool.")},
        {tr("2D System Editor"), tr("E"), tr("2D Canvas Focused"), tr("Switches to the editor rotation mode.")},
        {tr("2D System Editor"), tr("R"), tr("2D Canvas Focused"), tr("Switches to the editor scale mode.")},
        {tr("2D System Editor"), tr("Delete"), tr("System Editor Active"), tr("Deletes the current selection in the System Editor.")},
        {tr("2D System Editor"), tr("Ctrl+D"), tr("System Editor Active"), tr("Duplicates the current selection through the same clone/paste path as the editor.")},
        {tr("2D System Editor"), tr("Ctrl+C"), tr("System Editor Active"), tr("Copies the current object/zone selection to the internal editor clipboard.")},
        {tr("2D System Editor"), tr("Ctrl+V"), tr("System Editor Active"), tr("Pastes the last copied object/zone selection again with an offset.")},
        {tr("2D System Editor"), tr("Ctrl+A"), tr("System Editor Active"), tr("Selects all currently visible and filterable entries in the object tree.")},
        {tr("2D System Editor"), tr("G"), tr("2D Canvas Focused"), tr("Toggles the real NavMap grid.")},
        {tr("2D System Editor"), tr("F"), tr("2D Canvas Focused"), tr("Centers and frames the current selection. Without a selection, the entire map is fitted.")},
        {tr("2D System Editor"), tr("Esc"), tr("2D Canvas Focused"), tr("Safely cancels active placements or the ruler; otherwise switches back to the selection tool.")},
        {tr("2D System Editor"), tr("Arrow Keys"), tr("2D Canvas Focused"), tr("Moves the current selection finely by 100 Freelancer units.")},
        {tr("2D System Editor"), tr("Shift+Arrow Keys"), tr("2D Canvas Focused"), tr("Moves the current selection coarsely by 1000 Freelancer units.")},
        {tr("2D System Editor"), tr("Z"), tr("2D Canvas Focused"), tr("Rotates the current selection 15 degrees to the left.")},
        {tr("2D System Editor"), tr("X"), tr("2D Canvas Focused"), tr("Rotates the current selection 15 degrees to the right.")},

        {tr("While typing in search fields, text editors, number fields, or editable combo boxes, these editor shortcuts are not triggered."), tr("Text Input Protects Shortcuts"), tr("System Editor"), tr("While typing in search fields, text editors, number fields, or editable combo boxes, these editor shortcuts are not triggered.")},
        {tr("The shortcuts for E and R already switch the real editor tool state. Direct mouse manipulators for rotate/scale are not ported yet."), tr("Rotate/Scale Tool"), tr("2D System Editor"), tr("The shortcuts for E and R already switch the real editor tool state. Direct mouse manipulators for rotate/scale are not ported yet.")},
    };

    int rowCount = 0;
    QString currentSection;
    for (const ShortcutRow &row : rows) {
        if (row.section != currentSection) {
            currentSection = row.section;
            rowCount += 1;
        }
        rowCount += 1;
    }

    m_table->setRowCount(rowCount);

    int rowIndex = 0;
    currentSection.clear();
    for (const ShortcutRow &row : rows) {
        if (row.section != currentSection) {
            currentSection = row.section;
            addSectionHeaderRow(currentSection, rowIndex);
            ++rowIndex;
        }
        addShortcutRow(row, rowIndex);
        ++rowIndex;
    }

    m_table->resizeRowsToContents();
}

void KeyboardShortcutOverviewDialog::addSectionHeaderRow(const QString &title, int row)
{
    QFont font;
    font.setBold(true);
    font.setPointSize(font.pointSize() + 1);

    m_table->setSpan(row, 0, 1, 3);
    auto *item = makeReadOnlyItem(title, font);
    item->setBackground(palette().alternateBase());
    m_table->setItem(row, 0, item);
    m_table->setRowHeight(row, 30);
}

void KeyboardShortcutOverviewDialog::addShortcutRow(const ShortcutRow &row, int rowIndex)
{
    m_table->setItem(rowIndex, 0, makeReadOnlyItem(row.shortcut));
    m_table->setItem(rowIndex, 1, makeReadOnlyItem(row.context));
    auto *descriptionItem = makeReadOnlyItem(row.description);
    descriptionItem->setToolTip(row.description);
    m_table->setItem(rowIndex, 2, descriptionItem);
}

} // namespace flatlas::tools
