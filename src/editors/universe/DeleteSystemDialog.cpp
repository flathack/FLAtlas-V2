#include "DeleteSystemDialog.h"

#include "domain/UniverseData.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace flatlas::editors {

namespace {

QString htmlEscape(const QString &text)
{
    return text.toHtmlEscaped();
}

QString sectionTitle(const QString &title)
{
    return QStringLiteral("<h3 style=\"margin-bottom:4px;\">%1</h3>").arg(htmlEscape(title));
}

QString bulletList(const QStringList &items)
{
    if (items.isEmpty())
        return QStringLiteral("<p><i>Keine</i></p>");
    QString html = QStringLiteral("<ul>");
    for (const QString &item : items)
        html += QStringLiteral("<li>%1</li>").arg(htmlEscape(item));
    html += QStringLiteral("</ul>");
    return html;
}

} // namespace

DeleteSystemDialog::DeleteSystemDialog(const QString &universeFilePath,
                                       flatlas::domain::UniverseData *data,
                                       const QString &systemNickname,
                                       const QString &systemDisplayName,
                                       QWidget *parent)
    : QDialog(parent)
    , m_universeFilePath(universeFilePath)
    , m_data(data)
    , m_systemNickname(systemNickname.trimmed())
    , m_systemDisplayName(systemDisplayName.trimmed())
{
    setWindowTitle(tr("Delete System: %1").arg(m_systemDisplayName.isEmpty() ? m_systemNickname : m_systemDisplayName));
    resize(860, 720);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("Delete is blocked until Precheck completes successfully. "
           "The precheck lists universe.ini changes, dependent files, jump cleanup, kept shared files, warnings and blockers."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_reportView = new QTextBrowser(this);
    m_reportView->setOpenExternalLinks(false);
    layout->addWidget(m_reportView, 1);

    auto *buttons = new QDialogButtonBox(this);
    m_precheckButton = buttons->addButton(tr("Precheck"), QDialogButtonBox::ActionRole);
    m_deleteButton = buttons->addButton(tr("Delete System"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Close);
    m_deleteButton->setEnabled(false);

    connect(m_precheckButton, &QPushButton::clicked, this, &DeleteSystemDialog::runPrecheck);
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        runPrecheck();
        if (!m_report.canDelete)
            return;

        QString errorMessage;
        if (!DeleteSystemService::execute(m_report, m_data, &errorMessage)) {
            QMessageBox::warning(this, tr("Delete System"), errorMessage);
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_reportView->setHtml(QStringLiteral("<p><i>%1</i></p>")
                              .arg(htmlEscape(tr("No precheck executed yet."))));
}

void DeleteSystemDialog::runPrecheck()
{
    m_report = DeleteSystemService::precheck(m_universeFilePath, m_data, m_systemNickname);
    updateUiFromReport();
}

void DeleteSystemDialog::updateUiFromReport()
{
    m_reportView->setHtml(formatReportHtml());
    m_deleteButton->setEnabled(m_report.precheckCompleted && m_report.canDelete);
}

QString DeleteSystemDialog::formatReportHtml() const
{
    QString html;
    html += QStringLiteral("<html><body style=\"font-family:'Segoe UI';\">");
    html += sectionTitle(tr("Target"));
    html += QStringLiteral("<p><b>%1</b> (%2)</p>")
                .arg(htmlEscape(m_systemDisplayName.isEmpty() ? m_systemNickname : m_systemDisplayName),
                     htmlEscape(m_systemNickname));

    QStringList sectionLines;
    for (const auto &change : m_report.sectionChanges) {
        sectionLines.append(QStringLiteral("%1 | [%2] %3 | %4")
                                .arg(change.filePath, change.sectionName, change.identifier, change.reason));
    }
    html += sectionTitle(tr("Entries and Section Changes"));
    html += bulletList(sectionLines);

    QStringList deleteLines;
    for (const auto &entry : m_report.filesToDelete)
        deleteLines.append(QStringLiteral("%1 | %2").arg(entry.path, entry.reason));
    html += sectionTitle(tr("Files To Delete"));
    html += bulletList(deleteLines);

    QStringList keepLines;
    for (const auto &entry : m_report.filesToKeep)
        keepLines.append(QStringLiteral("%1 | %2").arg(entry.path, entry.reason));
    html += sectionTitle(tr("Files Kept"));
    html += bulletList(keepLines);

    QStringList jumpLines;
    for (const auto &entry : m_report.jumpCleanups) {
        QString text = QStringLiteral("%1 | remove object %2")
                           .arg(entry.systemFilePath, entry.objectNickname);
        if (!entry.zoneNickname.isEmpty())
            text += QStringLiteral(" | remove zone %1").arg(entry.zoneNickname);
        jumpLines.append(text);
    }
    html += sectionTitle(tr("Adjacent Jump Cleanup"));
    html += bulletList(jumpLines);

    html += sectionTitle(tr("Warnings"));
    html += bulletList(m_report.warnings);

    html += sectionTitle(tr("Blockers"));
    html += bulletList(m_report.blockers);

    html += sectionTitle(tr("Result"));
    html += QStringLiteral("<p><b>%1</b></p>")
                .arg(htmlEscape(m_report.canDelete
                                    ? tr("Precheck successful. Delete is enabled.")
                                    : tr("Precheck blocked. Delete stays disabled.")));
    html += QStringLiteral("</body></html>");
    return html;
}

} // namespace flatlas::editors
