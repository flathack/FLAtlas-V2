#include "FactionEditorPage.h"

#include "core/EditingContext.h"
#include "FactionReferenceDialog.h"
#include "infrastructure/freelancer/IdsDataService.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace flatlas::editors {

namespace {

QStringList linesFromEdit(const QTextEdit *edit)
{
    QStringList values;
    if (!edit)
        return values;
    for (const QString &line : edit->toPlainText().split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            values.append(trimmed);
    }
    return values;
}

void setLines(QTextEdit *edit, const QStringList &values)
{
    if (edit)
        edit->setPlainText(values.join(QLatin1Char('\n')));
}

QString severityText(flatlas::domain::FactionValidationSeverity severity)
{
    using Severity = flatlas::domain::FactionValidationSeverity;
    switch (severity) {
    case Severity::Critical: return QObject::tr("Critical");
    case Severity::Warning: return QObject::tr("Warning");
    case Severity::Info: return QObject::tr("Info");
    }
    return {};
}

QColor reputationColor(double value)
{
    if (value > 0.59)
        return QColor(42, 130, 78);
    if (value < -0.59)
        return QColor(150, 58, 50);
    return QColor(105, 110, 118);
}

} // namespace

FactionEditorPage::FactionEditorPage(QWidget *parent)
    : QWidget(parent)
    , m_service(new FactionEditorService(this))
{
    buildUi();
    connect(m_service, &FactionEditorService::worldChanged, this, [this]() {
        refreshFactionList();
        if (!m_currentNickname.isEmpty())
            loadFactionToEditors(m_currentNickname);
        refreshValidation();
        emit titleChanged(title());
    });
    connect(m_service, &FactionEditorService::dirtyChanged, this, [this](bool) {
        emit titleChanged(title());
    });
    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::contextChanged,
            this,
            [this](const QString &) { scheduleLoadFromContext(); });
    scheduleLoadFromContext();
}

bool FactionEditorPage::save(QString *errorMessage)
{
    saveEditorsToFaction();
    return m_service->save(errorMessage);
}

bool FactionEditorPage::isDirty() const
{
    return m_service->isDirty();
}

void FactionEditorPage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    root->addWidget(toolbar);
    toolbar->addAction(tr("New Faction"), this, [this]() {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("New Faction"));
        auto *layout = new QFormLayout(&dialog);
        auto *nicknameEdit = new QLineEdit(&dialog);
        auto *nameEdit = new QLineEdit(&dialog);
        auto *shortNameEdit = new QLineEdit(&dialog);
        auto *infocardEdit = new QTextEdit(&dialog);
        auto *templateCombo = new QComboBox(&dialog);
        auto *legalityEdit = new QLineEdit(&dialog);
        nicknameEdit->setPlaceholderText(QStringLiteral("fc_example_grp"));
        nameEdit->setPlaceholderText(tr("Ingame name"));
        shortNameEdit->setPlaceholderText(tr("Uses ingame name if empty"));
        infocardEdit->setPlaceholderText(tr("Uses ingame name if empty"));
        infocardEdit->setFixedHeight(72);
        legalityEdit->setText(QStringLiteral("lawful"));
        const QStringList nicknames = m_service->world().sortedNicknames();
        for (const QString &candidate : nicknames) {
            const auto *faction = m_service->world().faction(candidate);
            if (!faction || !faction->inFactionProp)
                continue;
            templateCombo->addItem(factionDisplayLabel(candidate), candidate);
        }
        if (templateCombo->count() == 0)
            templateCombo->addItem(tr("Minimal faction defaults"), QString());
        const int libertyIndex = templateCombo->findData(QStringLiteral("li_n_grp"));
        if (libertyIndex >= 0)
            templateCombo->setCurrentIndex(libertyIndex);
        else
            templateCombo->setCurrentIndex(0);
        layout->addRow(tr("Nickname"), nicknameEdit);
        layout->addRow(tr("Ingame name"), nameEdit);
        layout->addRow(tr("Short name"), shortNameEdit);
        layout->addRow(tr("Infocard text"), infocardEdit);
        layout->addRow(tr("Faction template"), templateCombo);
        layout->addRow(tr("Legality"), legalityEdit);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const QString nickname = nicknameEdit->text().trimmed();
        FactionCreationRequest request;
        request.nickname = nickname;
        request.ingameName = nameEdit->text().trimmed();
        request.shortName = shortNameEdit->text().trimmed();
        request.infocardText = infocardEdit->toPlainText().trimmed();
        request.templateNickname = templateCombo->currentData().toString();
        request.legality = legalityEdit->text().trimmed();
        saveEditorsToFaction();
        QString error;
        if (!m_service->addFaction(request, &error)) {
            QMessageBox::warning(this, tr("Faction Editor"), tr("Could not create faction:\n%1").arg(error));
            return;
        }
        refreshIdsTextCache();
        refreshFactionList();
        selectFaction(nickname);
    });
    auto *saveAction = toolbar->addAction(tr("Save"), this, [this]() {
        QString error;
        if (!save(&error))
            QMessageBox::warning(this, tr("Faction Editor"), tr("Could not save factions:\n%1").arg(error));
    });
    if (QWidget *saveButton = toolbar->widgetForAction(saveAction)) {
        saveButton->setStyleSheet(QStringLiteral(
            "QToolButton { background: #2f8f46; color: white; font-weight: 600; border: 1px solid #3fb95d; padding: 4px 10px; }"
            "QToolButton:hover { background: #36a653; }"
            "QToolButton:pressed { background: #26763a; }"));
    }
    toolbar->addAction(tr("Validate"), this, [this]() { saveEditorsToFaction(); refreshValidation(); });
    toolbar->addAction(tr("Reload"), this, [this]() { scheduleLoadFromContext(); });
    toolbar->addAction(tr("Deactivate"), this, [this]() {
        if (m_currentNickname.isEmpty())
            return;
        saveEditorsToFaction();
        FactionReferenceDialog dialog(m_service, m_currentNickname, this);
        connect(&dialog, &FactionReferenceDialog::factionChanged, this, [this](const QString &nickname) {
            refreshFactionList();
            refreshValidation();
            if (nickname.isEmpty()) {
                const QStringList nicknames = m_service->world().sortedNicknames();
                m_currentNickname.clear();
                if (!nicknames.isEmpty())
                    selectFaction(nicknames.first());
                return;
            }
            loadFactionToEditors(nickname);
        });
        dialog.exec();
    });

    m_pathLabel = new QLabel(this);
    root->addWidget(m_pathLabel);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    root->addWidget(splitter, 1);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_searchEdit = new QLineEdit(left);
    m_searchEdit->setPlaceholderText(tr("Search factions"));
    leftLayout->addWidget(m_searchEdit);
    m_factionList = new QListWidget(left);
    leftLayout->addWidget(m_factionList, 1);
    splitter->addWidget(left);

    auto *tabs = new QTabWidget(splitter);
    splitter->addWidget(tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 900});

    auto *general = new QWidget(tabs);
    auto *generalForm = new QFormLayout(general);
    m_nicknameEdit = new QLineEdit(general);
    m_nicknameEdit->setReadOnly(true);
    m_idsNameEdit = new QLineEdit(general);
    m_idsNameResolvedLabel = new QLabel(general);
    m_idsInfoEdit = new QLineEdit(general);
    m_idsInfoResolvedLabel = new QLabel(general);
    m_idsShortNameEdit = new QLineEdit(general);
    m_idsShortNameResolvedLabel = new QLabel(general);
    m_initialWorldCheck = new QCheckBox(tr("initialworld.ini"), general);
    m_empathyCheck = new QCheckBox(tr("empathy.ini"), general);
    m_factionPropCheck = new QCheckBox(tr("faction_prop.ini"), general);
    generalForm->addRow(tr("Nickname"), m_nicknameEdit);
    generalForm->addRow(tr("ids_name"), m_idsNameEdit);
    generalForm->addRow(tr("Resolved name"), m_idsNameResolvedLabel);
    generalForm->addRow(tr("ids_info"), m_idsInfoEdit);
    generalForm->addRow(tr("Resolved info"), m_idsInfoResolvedLabel);
    generalForm->addRow(tr("ids_short_name"), m_idsShortNameEdit);
    generalForm->addRow(tr("Resolved short name"), m_idsShortNameResolvedLabel);
    generalForm->addRow(tr("Present in"), m_initialWorldCheck);
    generalForm->addRow(QString(), m_empathyCheck);
    generalForm->addRow(QString(), m_factionPropCheck);
    tabs->addTab(general, tr("General"));

    auto *props = new QWidget(tabs);
    auto *propsForm = new QFormLayout(props);
    m_legalityEdit = new QLineEdit(props);
    m_pluralityEdit = new QLineEdit(props);
    m_msgPrefixEdit = new QLineEdit(props);
    m_jumpPreferenceEdit = new QLineEdit(props);
    m_npcShipsEdit = new QTextEdit(props);
    m_voicesEdit = new QTextEdit(props);
    m_spaceCostumesEdit = new QTextEdit(props);
    m_formationsEdit = new QTextEdit(props);
    propsForm->addRow(tr("legality"), m_legalityEdit);
    propsForm->addRow(tr("nickname_plurality"), m_pluralityEdit);
    propsForm->addRow(tr("msg_id_prefix"), m_msgPrefixEdit);
    propsForm->addRow(tr("jump_preference"), m_jumpPreferenceEdit);
    propsForm->addRow(tr("npc_ship"), m_npcShipsEdit);
    propsForm->addRow(tr("voice"), m_voicesEdit);
    propsForm->addRow(tr("space_costume"), m_spaceCostumesEdit);
    propsForm->addRow(tr("formation"), m_formationsEdit);
    tabs->addTab(props, tr("Faction Properties"));

    m_reputationTable = new QTableWidget(tabs);
    m_reputationTable->setColumnCount(4);
    m_reputationTable->setHorizontalHeaderLabels({tr("Faction"), tr("Ingame name"), tr("Value"), tr("Slider")});
    m_reputationTable->horizontalHeader()->setStretchLastSection(true);
    tabs->addTab(m_reputationTable, tr("Reputations"));

    m_empathyTable = new QTableWidget(tabs);
    m_empathyTable->setColumnCount(4);
    m_empathyTable->setHorizontalHeaderLabels({tr("Faction"), tr("Ingame name"), tr("Rate"), tr("Slider")});
    m_empathyTable->horizontalHeader()->setStretchLastSection(true);
    tabs->addTab(m_empathyTable, tr("Empathy"));

    m_validationTree = new QTreeWidget(tabs);
    m_validationTree->setHeaderLabels({tr("Severity"), tr("Faction"), tr("Message")});
    tabs->addTab(m_validationTree, tr("Validation"));

    connect(m_factionList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        saveEditorsToFaction();
        selectFaction(current ? current->data(Qt::UserRole).toString() : QString());
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { refreshFactionList(); });
    connect(m_idsNameEdit, &QLineEdit::textChanged, this, [this](const QString &value) {
        m_idsNameResolvedLabel->setText(resolvedIdsText(value));
    });
    connect(m_idsInfoEdit, &QLineEdit::textChanged, this, [this](const QString &value) {
        m_idsInfoResolvedLabel->setText(resolvedIdsText(value));
    });
    connect(m_idsShortNameEdit, &QLineEdit::textChanged, this, [this](const QString &value) {
        m_idsShortNameResolvedLabel->setText(resolvedIdsText(value));
    });
}

void FactionEditorPage::scheduleLoadFromContext()
{
    if (m_pathLabel)
        m_pathLabel->setText(tr("Faction-Daten werden geladen..."));
    QTimer::singleShot(0, this, &FactionEditorPage::loadFromContext);
}

void FactionEditorPage::loadFromContext()
{
    reportLoadingProgress(0, tr("Faction Editor: Daten werden vorbereitet..."));
    const QString root = flatlas::core::EditingContext::instance().primaryGamePath();
    m_pathLabel->setText(root.isEmpty() ? tr("No active editing context") : root);
    reportLoadingProgress(20, tr("Faction Editor: IDS-Texte werden geladen..."));
    refreshIdsTextCache();
    QString warnings;
    reportLoadingProgress(50, tr("Faction Editor: Faction-Dateien werden geladen..."));
    m_service->load(root, &warnings);
    if (!warnings.trimmed().isEmpty())
        m_pathLabel->setText(QStringLiteral("%1  -  %2").arg(m_pathLabel->text(), warnings.split(QLatin1Char('\n')).join(QStringLiteral("; "))));
    const QStringList nicknames = m_service->world().sortedNicknames();
    reportLoadingProgress(80, tr("Faction Editor: Tabellen werden gefuellt..."));
    if (!nicknames.isEmpty())
        selectFaction(nicknames.first());
    refreshFactionList();
    refreshValidation();
    emit titleChanged(title());
    reportLoadingProgress(100, tr("Faction Editor: %1 Factions geladen").arg(nicknames.size()));
}

void FactionEditorPage::reportLoadingProgress(int percent, const QString &message)
{
    emit loadingProgressChanged(std::clamp(percent, 0, 100), message);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void FactionEditorPage::refreshIdsTextCache()
{
    m_idsTextById.clear();
    const QString root = flatlas::core::EditingContext::instance().primaryGamePath();
    if (root.trimmed().isEmpty())
        return;
    const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(root);
    for (const auto &entry : dataset.entries) {
        const QString text = entry.plainText.trimmed().isEmpty()
            ? entry.stringValue.trimmed()
            : entry.plainText.trimmed();
        if (!text.isEmpty())
            m_idsTextById.insert(entry.globalId, text);
    }
}

void FactionEditorPage::refreshFactionList()
{
    QSignalBlocker blocker(m_factionList);
    const QString needle = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();
    m_factionList->clear();
    for (const QString &nickname : m_service->world().sortedNicknames()) {
        const auto *faction = m_service->world().faction(nickname);
        if (!faction)
            continue;
        const QString display = factionDisplayLabel(nickname);
        if (!needle.isEmpty()
            && !nickname.toLower().contains(needle)
            && !display.toLower().contains(needle))
            continue;
        QStringList markers;
        if (!faction->inInitialWorld) markers.append(QStringLiteral("!IW"));
        if (!faction->inEmpathy) markers.append(QStringLiteral("!EMP"));
        if (!faction->inFactionProp) markers.append(QStringLiteral("!FP"));
        auto *item = new QListWidgetItem(markers.isEmpty()
            ? display
            : QStringLiteral("%1  %2").arg(display, markers.join(QLatin1Char(' '))));
        item->setData(Qt::UserRole, nickname);
        if (!markers.isEmpty())
            item->setForeground(QColor(210, 150, 65));
        m_factionList->addItem(item);
        if (nickname.compare(m_currentNickname, Qt::CaseInsensitive) == 0)
            item->setSelected(true);
    }
}

void FactionEditorPage::selectFaction(const QString &nickname)
{
    if (nickname.trimmed().isEmpty())
        return;
    m_currentNickname = nickname;
    loadFactionToEditors(nickname);
    for (int i = 0; i < m_factionList->count(); ++i) {
        QListWidgetItem *item = m_factionList->item(i);
        if (item && item->data(Qt::UserRole).toString().compare(nickname, Qt::CaseInsensitive) == 0) {
            QSignalBlocker blocker(m_factionList);
            m_factionList->setCurrentItem(item);
            break;
        }
    }
}

void FactionEditorPage::loadFactionToEditors(const QString &nickname)
{
    const auto *faction = m_service->faction(nickname);
    if (!faction)
        return;
    m_updating = true;
    m_nicknameEdit->setText(faction->nickname);
    m_idsNameEdit->setText(faction->idsName);
    m_idsInfoEdit->setText(faction->idsInfo);
    m_idsShortNameEdit->setText(faction->idsShortName);
    m_idsNameResolvedLabel->setText(resolvedIdsText(faction->idsName));
    m_idsInfoResolvedLabel->setText(resolvedIdsText(faction->idsInfo));
    m_idsShortNameResolvedLabel->setText(resolvedIdsText(faction->idsShortName));
    m_initialWorldCheck->setChecked(faction->inInitialWorld);
    m_empathyCheck->setChecked(faction->inEmpathy);
    m_factionPropCheck->setChecked(faction->inFactionProp);
    m_legalityEdit->setText(faction->props.legality);
    m_pluralityEdit->setText(faction->props.nicknamePlurality);
    m_msgPrefixEdit->setText(faction->props.msgIdPrefix);
    m_jumpPreferenceEdit->setText(faction->props.jumpPreference);
    setLines(m_npcShipsEdit, faction->props.npcShips);
    setLines(m_voicesEdit, faction->props.voices);
    setLines(m_spaceCostumesEdit, faction->props.spaceCostumes);
    setLines(m_formationsEdit, faction->props.formations);
    m_updating = false;
    refreshReputationTable();
    refreshEmpathyTable();
}

void FactionEditorPage::saveEditorsToFaction()
{
    if (m_updating || m_currentNickname.isEmpty())
        return;
    auto *faction = m_service->faction(m_currentNickname);
    if (!faction)
        return;

    flatlas::domain::FactionPropData props = faction->props;
    props.affiliation = faction->nickname;
    props.legality = m_legalityEdit->text().trimmed();
    props.nicknamePlurality = m_pluralityEdit->text().trimmed();
    props.msgIdPrefix = m_msgPrefixEdit->text().trimmed();
    props.jumpPreference = m_jumpPreferenceEdit->text().trimmed();
    props.npcShips = linesFromEdit(m_npcShipsEdit);
    props.voices = linesFromEdit(m_voicesEdit);
    props.spaceCostumes = linesFromEdit(m_spaceCostumesEdit);
    props.formations = linesFromEdit(m_formationsEdit);
    m_service->setIds(m_currentNickname, m_idsNameEdit->text(), m_idsInfoEdit->text(), m_idsShortNameEdit->text());
    m_service->setProperties(m_currentNickname, props,
                             m_initialWorldCheck->isChecked(),
                             m_empathyCheck->isChecked(),
                             m_factionPropCheck->isChecked());
}

void FactionEditorPage::refreshReputationTable()
{
    const auto *faction = m_service->faction(m_currentNickname);
    if (!faction)
        return;
    QSignalBlocker blocker(m_reputationTable);
    const QStringList nicknames = m_service->world().sortedNicknames();
    m_reputationTable->setRowCount(nicknames.size());
    for (int row = 0; row < nicknames.size(); ++row) {
        const QString target = nicknames.at(row);
        const auto *targetFaction = m_service->world().faction(target);
        const double value = m_service->world().reputation(faction->nickname, target, 0.0);
        auto *nameItem = new QTableWidgetItem(target);
        auto *displayItem = new QTableWidgetItem(targetFaction ? resolvedIdsText(targetFaction->idsName) : QString());
        auto *valueItem = new QTableWidgetItem(QString::number(value, 'f', 3));
        valueItem->setForeground(reputationColor(value));
        m_reputationTable->setItem(row, 0, nameItem);
        m_reputationTable->setItem(row, 1, displayItem);
        m_reputationTable->setItem(row, 2, valueItem);
        auto *slider = new QSlider(Qt::Horizontal, m_reputationTable);
        slider->setRange(-10000, 10000);
        slider->setValue(static_cast<int>(value * 10000.0));
        connect(slider, &QSlider::valueChanged, this, [this, target, row](int sliderValue) {
            const double rep = static_cast<double>(sliderValue) / 10000.0;
            m_service->setReputation(m_currentNickname, target, rep);
            if (auto *item = m_reputationTable->item(row, 2)) {
                item->setText(QString::number(rep, 'f', 3));
                item->setForeground(reputationColor(rep));
            }
        });
        m_reputationTable->setCellWidget(row, 3, slider);
    }
}

void FactionEditorPage::refreshEmpathyTable()
{
    const auto *faction = m_service->faction(m_currentNickname);
    if (!faction)
        return;
    QSignalBlocker blocker(m_empathyTable);
    const QStringList nicknames = m_service->world().sortedNicknames();
    m_empathyTable->setRowCount(nicknames.size());
    for (int row = 0; row < nicknames.size(); ++row) {
        const QString target = nicknames.at(row);
        const auto *targetFaction = m_service->world().faction(target);
        const double value = m_service->world().empathyRate(faction->nickname, target, 0.0);
        m_empathyTable->setItem(row, 0, new QTableWidgetItem(target));
        m_empathyTable->setItem(row, 1, new QTableWidgetItem(targetFaction ? resolvedIdsText(targetFaction->idsName) : QString()));
        m_empathyTable->setItem(row, 2, new QTableWidgetItem(QString::number(value, 'f', 3)));
        auto *slider = new QSlider(Qt::Horizontal, m_empathyTable);
        slider->setRange(-10000, 10000);
        slider->setValue(static_cast<int>(value * 10000.0));
        connect(slider, &QSlider::valueChanged, this, [this, target, row](int sliderValue) {
            const double rate = static_cast<double>(sliderValue) / 10000.0;
            m_service->setEmpathyRate(m_currentNickname, target, rate);
            if (auto *item = m_empathyTable->item(row, 2))
                item->setText(QString::number(rate, 'f', 3));
        });
        m_empathyTable->setCellWidget(row, 3, slider);
    }
}

void FactionEditorPage::refreshValidation()
{
    m_validationTree->clear();
    for (const auto &issue : m_service->validate()) {
        auto *item = new QTreeWidgetItem(m_validationTree, {severityText(issue.severity), issue.faction, issue.message});
        if (issue.severity == flatlas::domain::FactionValidationSeverity::Critical)
            item->setForeground(0, QColor(210, 70, 70));
        else if (issue.severity == flatlas::domain::FactionValidationSeverity::Warning)
            item->setForeground(0, QColor(210, 150, 65));
    }
    m_validationTree->resizeColumnToContents(0);
    m_validationTree->resizeColumnToContents(1);
}

QString FactionEditorPage::resolvedIdsText(const QString &idsValue) const
{
    bool ok = false;
    const int id = idsValue.trimmed().toInt(&ok);
    if (!ok || id <= 0)
        return {};
    return m_idsTextById.value(id);
}

QString FactionEditorPage::factionDisplayLabel(const QString &nickname) const
{
    const auto *faction = m_service->world().faction(nickname);
    const QString ingameName = faction ? resolvedIdsText(faction->idsName) : QString();
    return ingameName.trimmed().isEmpty()
        ? nickname
        : QStringLiteral("%1  -  %2").arg(nickname, ingameName);
}

QString FactionEditorPage::title() const
{
    return m_service->isDirty() ? tr("Faction Editor*") : tr("Faction Editor");
}

} // namespace flatlas::editors
