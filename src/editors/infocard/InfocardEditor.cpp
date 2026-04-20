#include "InfocardEditor.h"
#include "infrastructure/parser/XmlInfocard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFont>

namespace flatlas::editors {

using flatlas::infrastructure::XmlInfocard;

InfocardEditor::InfocardEditor(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void InfocardEditor::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // --- Toolbar row ---
    auto *toolbar = new QHBoxLayout;
    m_validateBtn = new QPushButton(tr("Validate"));
    m_wrapBtn     = new QPushButton(tr("Wrap as Infocard"));
    auto *loadBtn = new QPushButton(tr("Open File..."));
    toolbar->addWidget(loadBtn);
    toolbar->addWidget(m_validateBtn);
    toolbar->addWidget(m_wrapBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    // --- Tab widget: Preview + XML Source ---
    m_tabs = new QTabWidget;

    // Preview tab
    m_preview = new QTextBrowser;
    m_preview->setOpenExternalLinks(false);
    m_preview->setStyleSheet(QStringLiteral(
        "QTextBrowser { background-color: #1e1e1e; color: #d4d4d4; }"));
    m_tabs->addTab(m_preview, tr("Preview"));

    // Source tab
    m_sourceEdit = new QPlainTextEdit;
    QFont mono(QStringLiteral("Consolas"), 10);
    mono.setStyleHint(QFont::Monospace);
    m_sourceEdit->setFont(mono);
    m_sourceEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_tabs->addTab(m_sourceEdit, tr("XML Source"));

    mainLayout->addWidget(m_tabs);

    // --- Status bar ---
    m_statusLabel = new QLabel;
    mainLayout->addWidget(m_statusLabel);

    // --- Connections ---
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 0)  // switching to Preview
            refreshPreview();
    });

    connect(m_sourceEdit, &QPlainTextEdit::textChanged,
            this, &InfocardEditor::onSourceChanged);

    connect(m_validateBtn, &QPushButton::clicked,
            this, &InfocardEditor::validateAndReport);

    connect(m_wrapBtn, &QPushButton::clicked, this, [this]() {
        const QString plain = m_sourceEdit->toPlainText();
        // Only wrap if it doesn't already look like XML
        if (!plain.trimmed().startsWith(QLatin1Char('<'))) {
            m_sourceEdit->setPlainText(XmlInfocard::wrapAsInfocard(plain));
        }
    });

    connect(loadBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Open Infocard"), QString(),
            tr("XML Files (*.xml);;All Files (*)"));
        if (!path.isEmpty())
            loadFromFile(path);
    });

    m_statusLabel->setText(tr("Ready"));
}

void InfocardEditor::loadInfocard(const QString &xml)
{
    m_sourceEdit->setPlainText(xml);
    m_modified = false;
    refreshPreview();

    // Extract title for tab
    auto data = XmlInfocard::parse(xml);
    if (!data.title.isEmpty())
        emit titleChanged(tr("Infocard: %1").arg(data.title));
    else
        emit titleChanged(tr("Infocard Editor"));
}

void InfocardEditor::loadFromFile(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusLabel->setText(tr("Cannot open: %1").arg(filePath));
        return;
    }
    QTextStream ts(&f);
    loadInfocard(ts.readAll());
    m_statusLabel->setText(tr("Loaded: %1").arg(filePath));
}

QString InfocardEditor::xmlSource() const
{
    return m_sourceEdit->toPlainText();
}

void InfocardEditor::refreshPreview()
{
    const QString xml = m_sourceEdit->toPlainText();
    if (xml.trimmed().isEmpty()) {
        m_preview->setHtml(QStringLiteral("<html><body><i>No infocard loaded</i></body></html>"));
        return;
    }

    auto data = XmlInfocard::parse(xml);
    m_preview->setHtml(XmlInfocard::toHtml(data));
}

void InfocardEditor::validateAndReport()
{
    QString error;
    if (XmlInfocard::validate(m_sourceEdit->toPlainText(), error)) {
        m_statusLabel->setText(tr("Valid infocard XML"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        m_statusLabel->setText(tr("Validation error: %1").arg(error));
        m_statusLabel->setStyleSheet(QStringLiteral("color: red;"));
    }
}

void InfocardEditor::onSourceChanged()
{
    m_modified = true;
    m_statusLabel->setStyleSheet({});
    m_statusLabel->setText(tr("Modified"));
}

} // namespace flatlas::editors
