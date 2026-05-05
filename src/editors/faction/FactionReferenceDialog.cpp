#include "FactionReferenceDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace flatlas::editors {

FactionReferenceDialog::FactionReferenceDialog(FactionEditorService *service,
                                               const QString &nickname,
                                               QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_nickname(nickname)
{
    setWindowTitle(tr("Deactivate / Delete Faction"));
    resize(980, 620);

    auto *layout = new QVBoxLayout(this);
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_replacementCombo = new QComboBox(this);
    m_replacementCombo->addItem(tr("Remove global relationships only"), QString());
    if (m_service) {
        for (const QString &candidate : m_service->world().sortedNicknames()) {
            if (candidate.compare(m_nickname, Qt::CaseInsensitive) != 0)
                m_replacementCombo->addItem(candidate, candidate);
        }
    }
    layout->addWidget(m_replacementCombo);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        tr("Scope"),
        tr("Source"),
        tr("Field"),
        tr("File"),
        tr("Line"),
        tr("Blocks Delete"),
        tr("Entry"),
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);

    auto *buttons = new QDialogButtonBox(this);
    m_deactivateButton = buttons->addButton(tr("Deactivate"), QDialogButtonBox::ActionRole);
    m_deleteButton = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(m_deactivateButton, &QPushButton::clicked, this, [this]() {
        if (runDeactivate())
            refresh();
    });
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        if (runDelete())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refresh();
}

QString FactionReferenceDialog::replacementNickname() const
{
    return m_replacementCombo ? m_replacementCombo->currentData().toString() : QString();
}

void FactionReferenceDialog::refresh()
{
    const QList<FactionReferenceRecord> records = m_service ? m_service->referencesForFaction(m_nickname) : QList<FactionReferenceRecord>{};
    int blockingCount = 0;
    int externalCount = 0;
    for (const auto &record : records) {
        if (record.blocksDelete)
            ++blockingCount;
        if (record.externalFileReference)
            ++externalCount;
    }

    QString summary = tr("%1 references found for %2. Delete is only allowed after all blocking relationships are removed or replaced.")
                          .arg(records.size())
                          .arg(m_nickname);
    if (externalCount > 0) {
        summary.append(QLatin1Char('\n'));
        summary.append(tr("%1 references are outside the global faction files and must be replaced with another faction. They will never be removed without replacement.")
                           .arg(externalCount));
    }
    m_summaryLabel->setText(summary);
    m_deleteButton->setEnabled(blockingCount == 0);
    m_deleteButton->setToolTip(blockingCount == 0
        ? QString()
        : tr("%1 blocking relationships still exist. Deactivate with remove/replace first.").arg(blockingCount));

    m_table->setRowCount(records.size());
    for (int row = 0; row < records.size(); ++row) {
        const auto &record = records.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(record.scope));
        m_table->setItem(row, 1, new QTableWidgetItem(record.source));
        m_table->setItem(row, 2, new QTableWidgetItem(record.field));
        m_table->setItem(row, 3, new QTableWidgetItem(record.filePath));
        m_table->setItem(row, 4, new QTableWidgetItem(record.lineNumber > 0 ? QString::number(record.lineNumber) : QString()));
        m_table->setItem(row, 5, new QTableWidgetItem(record.blocksDelete ? tr("Yes") : tr("No")));
        m_table->setItem(row, 6, new QTableWidgetItem(record.text));
    }
    m_table->resizeColumnsToContents();
}

bool FactionReferenceDialog::runDeactivate()
{
    if (!m_service)
        return false;
    const QString replacement = replacementNickname();
    const QString actionText = replacement.isEmpty()
        ? tr("remove global relationships to %1").arg(m_nickname)
        : tr("replace all listed references to %1 with %2").arg(m_nickname, replacement);
    if (QMessageBox::question(this,
                              tr("Deactivate Faction"),
                              tr("Deactivate %1 and %2?").arg(m_nickname, actionText))
        != QMessageBox::Yes) {
        return false;
    }

    QString error;
    if (!m_service->deactivateFaction(m_nickname, replacement, &error)) {
        QMessageBox::warning(this, tr("Faction Editor"), tr("Could not deactivate faction:\n%1").arg(error));
        return false;
    }
    emit factionChanged(m_nickname);
    return true;
}

bool FactionReferenceDialog::runDelete()
{
    if (!m_service)
        return false;
    const QString replacement = replacementNickname();
    if (QMessageBox::warning(this,
                             tr("Delete Faction"),
                             tr("Delete %1 permanently from the faction world?").arg(m_nickname),
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)
        != QMessageBox::Yes) {
        return false;
    }

    QString error;
    if (!m_service->deleteFaction(m_nickname, replacement, &error)) {
        QMessageBox::warning(this, tr("Faction Editor"), tr("Could not delete faction:\n%1").arg(error));
        refresh();
        return false;
    }
    emit factionChanged(QString());
    return true;
}

} // namespace flatlas::editors
