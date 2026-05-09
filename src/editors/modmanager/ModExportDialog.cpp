#include "ModExportDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace flatlas::editors {

ModExportDialog::ModExportDialog(const QString &profileName,
                                 const QString &modRoot,
                                 const QString &referenceRoot,
                                 const QString &defaultDirectory,
                                 QWidget *parent)
    : QDialog(parent)
{
    setupUi(profileName, modRoot, referenceRoot, defaultDirectory);
}

QString ModExportDialog::targetPath() const
{
    return m_targetEdit ? m_targetEdit->text().trimmed() : QString();
}

void ModExportDialog::setupUi(const QString &profileName,
                              const QString &modRoot,
                              const QString &referenceRoot,
                              const QString &defaultDirectory)
{
    setWindowTitle(tr("Mod exportieren"));
    resize(940, 680);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    m_summaryLabel = new QLabel(tr("No scan has been run yet. Click Scan to find new and changed files."), this);
    m_summaryLabel->setWordWrap(true);
    root->addWidget(m_summaryLabel);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto makePathRow = [this](QLineEdit **edit, const QString &value, auto slot) {
        auto *host = new QWidget(this);
        auto *layout = new QHBoxLayout(host);
        layout->setContentsMargins(0, 0, 0, 0);
        *edit = new QLineEdit(value, host);
        auto *button = new QPushButton(tr("Durchsuchen..."), host);
        connect(button, &QPushButton::clicked, this, slot);
        layout->addWidget(*edit, 1);
        layout->addWidget(button);
        return host;
    };

    form->addRow(tr("Mod Source:"), makePathRow(&m_modRootEdit, modRoot, &ModExportDialog::chooseModRoot));
    form->addRow(tr("Referenz:"), makePathRow(&m_referenceRootEdit, referenceRoot, &ModExportDialog::chooseReferenceRoot));

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("FLMOD"), QStringLiteral("flmod"));
    m_formatCombo->addItem(tr("ZIP"), QStringLiteral("zip"));
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &ModExportDialog::updateTargetSuffix);
    form->addRow(tr("Format:"), m_formatCombo);

    const QString safeName = profileName.trimmed().isEmpty() ? QStringLiteral("FLAtlas-Export") : profileName.trimmed();
    const QString defaultTarget = QDir(defaultDirectory.trimmed().isEmpty() ? QDir::homePath() : defaultDirectory)
        .filePath(safeName + QStringLiteral(".flmod"));
    form->addRow(tr("Speicherort:"), makePathRow(&m_targetEdit, defaultTarget, &ModExportDialog::chooseTargetPath));
    root->addLayout(form);

    auto *scriptForm = new QFormLayout;
    m_nameEdit = new QLineEdit(safeName, this);
    m_authorEdit = new QLineEdit(this);
    m_saveSafeCheck = new QCheckBox(tr("Savegame-sicher"), this);
    m_saveSafeCheck->setChecked(true);
    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setMaximumHeight(72);
    scriptForm->addRow(tr("Mod-Name:"), m_nameEdit);
    scriptForm->addRow(tr("Autor:"), m_authorEdit);
    scriptForm->addRow(QString(), m_saveSafeCheck);
    scriptForm->addRow(tr("Beschreibung:"), m_descriptionEdit);
    root->addLayout(scriptForm);

    auto *tabs = new QTabWidget(this);
    m_fileTable = new QTableWidget(tabs);
    m_fileTable->setColumnCount(5);
    m_fileTable->setHorizontalHeaderLabels({tr("Status"), tr("Pfad"), tr("Größe"), tr("SHA-256"), tr("Aktion")});
    m_fileTable->setAlternatingRowColors(true);
    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->verticalHeader()->setVisible(false);
    m_fileTable->horizontalHeader()->setStretchLastSection(false);
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tabs->addTab(m_fileTable, tr("Files"));

    m_scriptEdit = new QPlainTextEdit(tabs);
    m_scriptEdit->setPlainText(ModExportService::defaultScriptXml(safeName, {}, {}, true));
    connect(m_scriptEdit, &QPlainTextEdit::textChanged, this, [this]() { m_scriptManuallyEdited = true; });
    tabs->addTab(m_scriptEdit, QStringLiteral("script.xml"));
    root->addWidget(tabs, 1);

    auto *actions = new QHBoxLayout;
    auto *scanButton = new QPushButton(tr("Scannen"), this);
    auto *exclusionsButton = new QPushButton(tr("Exclusions"), this);
    auto *regenScriptButton = new QPushButton(tr("script.xml aus Feldern aktualisieren"), this);
    m_exportButton = new QPushButton(tr("Export"), this);
    m_exportButton->setEnabled(false);
    actions->addWidget(scanButton);
    actions->addWidget(exclusionsButton);
    actions->addWidget(regenScriptButton);
    actions->addStretch();
    actions->addWidget(m_exportButton);
    root->addLayout(actions);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    root->addWidget(buttons);

    connect(scanButton, &QPushButton::clicked, this, &ModExportDialog::scan);
    connect(exclusionsButton, &QPushButton::clicked, this, &ModExportDialog::showExclusions);
    connect(regenScriptButton, &QPushButton::clicked, this, [this]() {
        m_scriptManuallyEdited = false;
        refreshScriptXml();
    });
    connect(m_exportButton, &QPushButton::clicked, this, &ModExportDialog::exportArchive);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &ModExportDialog::refreshScriptXml);
    connect(m_authorEdit, &QLineEdit::textChanged, this, &ModExportDialog::refreshScriptXml);
    connect(m_saveSafeCheck, &QCheckBox::toggled, this, &ModExportDialog::refreshScriptXml);
    connect(m_descriptionEdit, &QPlainTextEdit::textChanged, this, &ModExportDialog::refreshScriptXml);
}

void ModExportDialog::chooseModRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose Mod Source"), m_modRootEdit->text());
    if (!dir.isEmpty())
        m_modRootEdit->setText(dir);
}

void ModExportDialog::chooseReferenceRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Referenzinstallation wählen"), m_referenceRootEdit->text());
    if (!dir.isEmpty())
        m_referenceRootEdit->setText(dir);
}

void ModExportDialog::chooseTargetPath()
{
    const QString format = m_formatCombo->currentData().toString();
    const QString filter = format == QStringLiteral("zip")
        ? tr("ZIP (*.zip);;All Files (*)")
        : tr("FLMOD (*.flmod);;All Files (*)");
    QString path = QFileDialog::getSaveFileName(this, tr("Export speichern"), m_targetEdit->text(), filter);
    if (!path.isEmpty()) {
        m_targetEdit->setText(path);
        updateTargetSuffix();
    }
}

void ModExportDialog::scan()
{
    QProgressDialog progress(tr("Comparing mod files..."), tr("Abbrechen"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    m_manualExclusions.clear();
    m_plan = ModExportService::collectChangedFiles(
        m_modRootEdit->text(),
        m_referenceRootEdit->text(),
        [&progress](const QString &, int current, int total, const QString &path) {
            progress.setMaximum(std::max(1, total));
            progress.setValue(current);
            progress.setLabelText(QObject::tr("Prüfe: %1").arg(path));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            return !progress.wasCanceled();
        });
    m_hasPlan = true;

    progress.setValue(progress.maximum());
    refreshFileTable();
    refreshSummary();
    m_exportButton->setEnabled(!m_plan.exportFiles().isEmpty());

    if (!m_plan.errors.isEmpty()) {
        QMessageBox::warning(this, tr("Mod exportieren"), m_plan.errors.join(QLatin1Char('\n')));
    } else if (m_plan.exportFiles().isEmpty()) {
        QMessageBox::information(this, tr("No new or changed files found."), tr("No new or changed files found."));
    }
}

void ModExportDialog::exportArchive()
{
    if (!m_hasPlan) {
        QMessageBox::information(this, tr("Mod exportieren"), tr("Please scan first to determine exportable files."));
        return;
    }

    updateTargetSuffix();
    const QString target = targetPath();
    if (target.isEmpty()) {
        chooseTargetPath();
        if (targetPath().isEmpty())
            return;
    }
    if (QFileInfo::exists(targetPath())) {
        const auto answer = QMessageBox::question(this,
                                                  tr("Mod exportieren"),
                                                  tr("File already exists. Overwrite?\n%1").arg(targetPath()));
        if (answer != QMessageBox::Yes)
            return;
    }

    const ModExportPlan plan = filteredPlan();
    if (plan.exportFiles().isEmpty()) {
        QMessageBox::information(this, tr("No files selected for export."), tr("No files selected for export."));
        return;
    }

    QProgressDialog progress(tr("Export wird geschrieben..."), tr("Abbrechen"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    QString error;
    const QString format = m_formatCombo->currentData().toString();
    const bool ok = format == QStringLiteral("zip")
        ? ModExportService::writeZip(plan, targetPath(), &error, [&progress](const QString &, int current, int total, const QString &path) {
              progress.setMaximum(std::max(1, total));
              progress.setValue(current);
              progress.setLabelText(QObject::tr("Schreibe: %1").arg(path));
              QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
              return !progress.wasCanceled();
          })
        : ModExportService::writeFlmod(plan, targetPath(), m_scriptEdit->toPlainText(), &error,
                                       [&progress](const QString &, int current, int total, const QString &path) {
              progress.setMaximum(std::max(1, total));
              progress.setValue(current);
              progress.setLabelText(QObject::tr("Schreibe: %1").arg(path));
              QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
              return !progress.wasCanceled();
          });

    if (!ok) {
        QMessageBox::warning(this, tr("Mod exportieren"),
                             error.trimmed().isEmpty() ? tr("Export could not be written.") : error);
        return;
    }

    m_exportedCount = plan.exportFiles().size();
    QMessageBox::information(this,
                             tr("Mod exportieren"),
                             tr("Export erstellt: %1\nDateien: %2").arg(targetPath()).arg(m_exportedCount));
    accept();
}

void ModExportDialog::showExclusions()
{
    QStringList lines;
    lines << tr("Automatisch ausgeschlossen:");
    for (const QString &label : ModExportService::automaticExclusionLabels())
        lines << QStringLiteral("  %1").arg(label);
    lines << QString();
    lines << tr("Manuell ausgeschlossen:");
    if (m_manualExclusions.isEmpty()) {
        lines << tr("  None");
    } else {
        QStringList manual = m_manualExclusions.values();
        std::sort(manual.begin(), manual.end(), [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        for (const QString &path : manual)
            lines << QStringLiteral("  %1").arg(path);
    }
    QMessageBox::information(this, tr("Exclusions"), lines.join(QLatin1Char('\n')));
}

void ModExportDialog::refreshFileTable()
{
    const QVector<ModExportFile> files = m_plan.exportFiles();
    m_fileTable->setRowCount(files.size());
    for (int row = 0; row < files.size(); ++row) {
        const ModExportFile &file = files.at(row);
        const bool excluded = m_manualExclusions.contains(file.relativePath);
        auto *statusItem = new QTableWidgetItem(excluded ? tr("Excluded") : file.status);
        statusItem->setData(Qt::UserRole, file.relativePath);
        m_fileTable->setItem(row, 0, statusItem);
        m_fileTable->setItem(row, 1, new QTableWidgetItem(file.relativePath));
        m_fileTable->setItem(row, 2, new QTableWidgetItem(QString::number(file.size)));
        m_fileTable->setItem(row, 3, new QTableWidgetItem(file.sha256.left(12)));

        auto *button = new QPushButton(excluded ? tr("Include") : tr("Exclude"), m_fileTable);
        connect(button, &QPushButton::clicked, this, [this, path = file.relativePath]() {
            if (m_manualExclusions.contains(path))
                m_manualExclusions.remove(path);
            else
                m_manualExclusions.insert(path);
            refreshFileTable();
            refreshSummary();
        });
        m_fileTable->setCellWidget(row, 4, button);
    }
    m_fileTable->resizeColumnsToContents();
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void ModExportDialog::refreshSummary()
{
    if (!m_hasPlan) {
        m_summaryLabel->setText(tr("No scan has been run yet. Click Scan to find new and changed files."));
        return;
    }
    const ModExportPlan plan = filteredPlan();
    m_summaryLabel->setText(tr("Exportierbare Dateien: %1 neu, %2 geändert. Unverändert: %3. Manuell ausgeschlossen: %4.")
                                .arg(plan.newCount())
                                .arg(plan.modifiedCount())
                                .arg(m_plan.unchangedCount)
                                .arg(m_manualExclusions.size()));
    m_exportButton->setEnabled(!plan.exportFiles().isEmpty());
}

void ModExportDialog::refreshScriptXml()
{
    if (m_scriptManuallyEdited)
        return;
    const QString xml = ModExportService::defaultScriptXml(m_nameEdit->text(),
                                                           m_authorEdit->text(),
                                                           m_descriptionEdit->toPlainText(),
                                                           m_saveSafeCheck->isChecked());
    QSignalBlocker blocker(m_scriptEdit);
    m_scriptEdit->setPlainText(xml);
}

void ModExportDialog::updateTargetSuffix()
{
    if (!m_targetEdit)
        return;
    const QString suffix = m_formatCombo->currentData().toString() == QStringLiteral("zip")
        ? QStringLiteral(".zip")
        : QStringLiteral(".flmod");
    QString path = m_targetEdit->text().trimmed();
    if (path.isEmpty())
        return;
    if (path.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral(".flmod"), Qt::CaseInsensitive)) {
        path = path.left(path.lastIndexOf(QLatin1Char('.')));
    }
    m_targetEdit->setText(path + suffix);
}

ModExportPlan ModExportDialog::filteredPlan() const
{
    return ModExportService::filterPlan(m_plan, m_manualExclusions);
}

} // namespace flatlas::editors
