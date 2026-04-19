// editors/npc/NpcEditorPage.cpp – NPC-Editor (Phase 14)

#include "NpcEditorPage.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QFileDialog>

namespace flatlas::editors {

NpcEditorPage::NpcEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void NpcEditorPage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    m_npcTable = new QTableWidget(this);
    m_npcTable->setColumnCount(6);
    m_npcTable->setHorizontalHeaderLabels({
        tr("Nickname"), tr("Room"), tr("Faction"),
        tr("Type"), tr("IDS Name"), tr("IDS Info")
    });
    m_npcTable->horizontalHeader()->setStretchLastSection(true);
    m_npcTable->setAlternatingRowColors(true);
    m_npcTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_npcTable->verticalHeader()->setVisible(false);
    layout->addWidget(m_npcTable);

    m_statusLabel = new QLabel(tr("Load an MBases.ini file to begin"), this);
    layout->addWidget(m_statusLabel);
}

void NpcEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);

    m_toolBar->addAction(tr("Load..."), this, &NpcEditorPage::onLoadClicked);
    m_toolBar->addAction(tr("Save"), this, &NpcEditorPage::onSaveClicked);
    m_toolBar->addSeparator();

    m_toolBar->addWidget(new QLabel(tr(" Base: "), m_toolBar));
    m_baseSelector = new QComboBox(m_toolBar);
    connect(m_baseSelector, &QComboBox::currentIndexChanged,
            this, &NpcEditorPage::onBaseSelected);
    m_toolBar->addWidget(m_baseSelector);
}

void NpcEditorPage::loadFile(const QString &filePath)
{
    m_filePath = filePath;
    m_bases = MbaseOperations::parseFile(filePath);

    refreshBaseSelector();

    m_statusLabel->setText(tr("Loaded %1 bases, %2 NPCs from %3")
        .arg(m_bases.size()).arg(npcCount()).arg(QFileInfo(filePath).fileName()));
    emit titleChanged(QStringLiteral("NPC Editor – %1").arg(QFileInfo(filePath).fileName()));
}

bool NpcEditorPage::saveFile(const QString &filePath)
{
    // Apply table edits back to data
    if (m_currentBaseIndex >= 0 && m_currentBaseIndex < m_bases.size()) {
        auto &base = m_bases[m_currentBaseIndex];
        for (int row = 0; row < m_npcTable->rowCount() && row < base.npcs.size(); ++row) {
            auto item = [&](int col) { return m_npcTable->item(row, col); };
            if (item(0)) base.npcs[row].nickname = item(0)->text();
            if (item(1)) base.npcs[row].room = item(1)->text();
            if (item(2)) base.npcs[row].faction = item(2)->text();
            if (item(3)) base.npcs[row].type = item(3)->text();
            if (item(4)) base.npcs[row].idsName = item(4)->text().toInt();
            if (item(5)) base.npcs[row].idsInfo = item(5)->text().toInt();
        }
    }

    QString text = MbaseOperations::serialize(m_bases);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(text.toUtf8());
    m_statusLabel->setText(tr("Saved to %1").arg(filePath));
    return true;
}

int NpcEditorPage::npcCount() const
{
    int count = 0;
    for (const auto &base : m_bases)
        count += base.npcs.size();
    return count;
}

void NpcEditorPage::refreshBaseSelector()
{
    m_baseSelector->blockSignals(true);
    m_baseSelector->clear();
    for (const auto &base : m_bases) {
        m_baseSelector->addItem(QStringLiteral("%1 (%2 NPCs)")
            .arg(base.baseName).arg(base.npcs.size()));
    }
    m_baseSelector->blockSignals(false);

    if (!m_bases.isEmpty())
        onBaseSelected(0);
}

void NpcEditorPage::refreshNpcTable()
{
    if (m_currentBaseIndex < 0 || m_currentBaseIndex >= m_bases.size()) {
        m_npcTable->setRowCount(0);
        return;
    }

    const auto &npcs = m_bases[m_currentBaseIndex].npcs;
    m_npcTable->setRowCount(npcs.size());

    for (int i = 0; i < npcs.size(); ++i) {
        const auto &npc = npcs[i];
        m_npcTable->setItem(i, 0, new QTableWidgetItem(npc.nickname));
        m_npcTable->setItem(i, 1, new QTableWidgetItem(npc.room));
        m_npcTable->setItem(i, 2, new QTableWidgetItem(npc.faction));
        m_npcTable->setItem(i, 3, new QTableWidgetItem(npc.type));
        m_npcTable->setItem(i, 4, new QTableWidgetItem(
            npc.idsName > 0 ? QString::number(npc.idsName) : QString()));
        m_npcTable->setItem(i, 5, new QTableWidgetItem(
            npc.idsInfo > 0 ? QString::number(npc.idsInfo) : QString()));
    }

    m_npcTable->resizeColumnsToContents();
}

void NpcEditorPage::onBaseSelected(int index)
{
    m_currentBaseIndex = index;
    refreshNpcTable();
}

void NpcEditorPage::onLoadClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Load MBases.ini"), QString(),
        tr("INI files (*.ini);;All files (*.*)"));
    if (!path.isEmpty())
        loadFile(path);
}

void NpcEditorPage::onSaveClicked()
{
    if (m_filePath.isEmpty()) {
        QString path = QFileDialog::getSaveFileName(
            this, tr("Save MBases.ini"), QStringLiteral("mbases.ini"),
            tr("INI files (*.ini);;All files (*.*)"));
        if (path.isEmpty()) return;
        m_filePath = path;
    }
    saveFile(m_filePath);
}

} // namespace flatlas::editors
