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
        tr("Diese Übersicht bleibt als eigenes Fenster geöffnet und kann auf einen zweiten Monitor verschoben werden. "
           "Die 2D-Systemeditor-Shortcuts gelten nur, wenn der System Editor aktiv ist. Canvas-Aktionen wie Toolwechsel, "
           "Bewegen, Rotieren, Fokus oder Abbrechen benötigen zusätzlich den Fokus im 2D-Canvas und feuern nicht während der Texteingabe."),
        this);
    m_introLabel->setWordWrap(true);
    layout->addWidget(m_introLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Shortcut"), tr("Kontext"), tr("Aktion")});
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
        {tr("Allgemein"), tr("Ctrl+S"), tr("Aktiver Editor"), tr("Speichert die aktuelle Datei bzw. den aktuellen Editorzustand.")},
        {tr("Allgemein"), tr("Ctrl+Z"), tr("Aktiver Editor"), tr("Macht die letzte Undo-fähige Aktion rückgängig.")},
        {tr("Allgemein"), tr("Ctrl+Y"), tr("Aktiver Editor"), tr("Stellt die letzte rückgängig gemachte Aktion wieder her.")},
        {tr("Allgemein"), tr("Ctrl+Shift+Z"), tr("Aktiver Editor"), tr("Alternative Redo-Tastenkombination.")},
        {tr("Allgemein"), tr("F1"), tr("Gesamte Anwendung"), tr("Öffnet die kontextsensitive Hilfe für den aktuellen Editor oder Dialog.")},

        {tr("2D-Systemeditor"), tr("Q oder V"), tr("2D-Canvas fokussiert"), tr("Schaltet auf das Auswahlwerkzeug.")},
        {tr("2D-Systemeditor"), tr("W"), tr("2D-Canvas fokussiert"), tr("Schaltet auf das Verschiebewerkzeug.")},
        {tr("2D-Systemeditor"), tr("E"), tr("2D-Canvas fokussiert"), tr("Schaltet auf den Rotationsmodus des Editors.")},
        {tr("2D-Systemeditor"), tr("R"), tr("2D-Canvas fokussiert"), tr("Schaltet auf den Skalierungsmodus des Editors.")},
        {tr("2D-Systemeditor"), tr("Delete"), tr("System Editor aktiv"), tr("Löscht die aktuelle Auswahl im System Editor.")},
        {tr("2D-Systemeditor"), tr("Ctrl+D"), tr("System Editor aktiv"), tr("Dupliziert die aktuelle Auswahl über denselben Clone-/Paste-Pfad wie der Editor.")},
        {tr("2D-Systemeditor"), tr("Ctrl+C"), tr("System Editor aktiv"), tr("Kopiert die aktuelle Objekt-/Zonenauswahl in die interne Editor-Zwischenablage.")},
        {tr("2D-Systemeditor"), tr("Ctrl+V"), tr("System Editor aktiv"), tr("Fügt die zuletzt kopierte Objekt-/Zonenauswahl mit Versatz wieder ein.")},
        {tr("2D-Systemeditor"), tr("Ctrl+A"), tr("System Editor aktiv"), tr("Wählt alle aktuell sichtbaren und filterbaren Einträge im Objektbaum aus.")},
        {tr("2D-Systemeditor"), tr("G"), tr("2D-Canvas fokussiert"), tr("Schaltet das echte NavMap-Grid ein oder aus.")},
        {tr("2D-Systemeditor"), tr("F"), tr("2D-Canvas fokussiert"), tr("Zentriert und framed die aktuelle Auswahl. Ohne Auswahl wird auf die gesamte Map gefittet.")},
        {tr("2D-Systemeditor"), tr("Esc"), tr("2D-Canvas fokussiert"), tr("Bricht aktive Platzierungen oder den Ruler sicher ab; andernfalls Wechsel zurück zum Auswahlwerkzeug.")},
        {tr("2D-Systemeditor"), tr("Pfeiltasten"), tr("2D-Canvas fokussiert"), tr("Verschiebt die aktuelle Auswahl fein um 100 Freelancer-Einheiten.")},
        {tr("2D-Systemeditor"), tr("Shift+Pfeiltasten"), tr("2D-Canvas fokussiert"), tr("Verschiebt die aktuelle Auswahl grob um 1000 Freelancer-Einheiten.")},
        {tr("2D-Systemeditor"), tr("Z"), tr("2D-Canvas fokussiert"), tr("Rotiert die aktuelle Auswahl um 15 Grad nach links.")},
        {tr("2D-Systemeditor"), tr("X"), tr("2D-Canvas fokussiert"), tr("Rotiert die aktuelle Auswahl um 15 Grad nach rechts.")},

        {tr("Hinweise"), tr("Texteingabe schützt Shortcuts"), tr("System Editor"), tr("Während der Eingabe in Suchfeldern, Texteditoren, Zahlenfeldern oder editierbaren Comboboxen werden diese Editor-Shortcuts nicht ausgelöst.")},
        {tr("Hinweise"), tr("Rotate/Scale-Tool"), tr("2D-Systemeditor"), tr("Die Tastenkürzel für E und R schalten bereits den echten Editor-Tool-State um. Direkte Maus-Manipulatoren für Rotate/Scale sind noch nicht portiert.")},
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