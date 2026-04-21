#include "EditSystemDialog.h"

#include "domain/UniverseData.h"
#include "infrastructure/parser/XmlInfocard.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace flatlas::editors {

using flatlas::domain::SystemInfo;
using flatlas::infrastructure::XmlInfocard;

EditSystemDialog::EditSystemDialog(const SystemInfo &systemInfo,
                                   const QString &currentResolvedName,
                                   const QString &currentInfocardXml,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("System bearbeiten: %1").arg(systemInfo.nickname));
    resize(720, 620);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_visitCombo = new QComboBox(this);
    m_visitCombo->setEditable(true);
    m_visitCombo->addItem(QStringLiteral("0 - normal"), 0);
    m_visitCombo->addItem(QStringLiteral("1 - always visible"), 1);
    m_visitCombo->addItem(QStringLiteral("128 - hidden system"), 128);
    const int visitIndex = m_visitCombo->findData(systemInfo.visit);
    if (visitIndex >= 0)
        m_visitCombo->setCurrentIndex(visitIndex);
    else
        m_visitCombo->setCurrentText(QString::number(systemInfo.visit));
    form->addRow(tr("Visit:"), m_visitCombo);

    auto *visitHelp = new QLabel(
        tr("0 = normal, 1 = always visible, 128 = hidden system"), this);
    visitHelp->setWordWrap(true);
    form->addRow(QString(), visitHelp);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(currentResolvedName.trimmed().isEmpty() ? systemInfo.nickname : currentResolvedName.trimmed());
    form->addRow(tr("Name:"), m_nameEdit);

    m_navMapScaleSpin = new QDoubleSpinBox(this);
    m_navMapScaleSpin->setDecimals(6);
    m_navMapScaleSpin->setRange(0.000001, 1000.0);
    m_navMapScaleSpin->setValue(systemInfo.navMapScale > 0.0 ? systemInfo.navMapScale : 1.36);
    form->addRow(tr("NavMapScale:"), m_navMapScaleSpin);

    layout->addLayout(form);

    auto *toolbar = new QHBoxLayout();
    m_wrapInfocardButton = new QPushButton(tr("Als Infocard erzeugen"), this);
    toolbar->addWidget(m_wrapInfocardButton);
    toolbar->addStretch(1);
    layout->addLayout(toolbar);

    auto *infocardLabel = new QLabel(tr("Infocard XML:"), this);
    layout->addWidget(infocardLabel);

    m_infocardEdit = new QPlainTextEdit(this);
    m_infocardEdit->setPlainText(currentInfocardXml.trimmed());
    layout->addWidget(m_infocardEdit, 1);

    auto *previewTitle = new QLabel(tr("Vorschau:"), this);
    layout->addWidget(previewTitle);

    m_infocardPreview = new QLabel(this);
    m_infocardPreview->setWordWrap(true);
    m_infocardPreview->setMinimumHeight(80);
    layout->addWidget(m_infocardPreview);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_infocardEdit, &QPlainTextEdit::textChanged, this, &EditSystemDialog::updateInfocardPreview);
    connect(m_wrapInfocardButton, &QPushButton::clicked, this, [this]() {
        const QString xml = XmlInfocard::wrapAsInfocard(m_infocardEdit->toPlainText().trimmed());
        m_infocardEdit->setPlainText(xml);
    });

    updateInfocardPreview();
}

EditSystemRequest EditSystemDialog::request() const
{
    EditSystemRequest req;
    req.visit = (m_visitCombo->currentData().isValid()
                     ? m_visitCombo->currentData().toInt()
                     : m_visitCombo->currentText().split(QLatin1Char(' ')).value(0).trimmed().toInt());
    req.name = m_nameEdit->text().trimmed();
    req.infocardXml = m_infocardEdit->toPlainText().trimmed();
    req.navMapScale = m_navMapScaleSpin->value();
    return req;
}

void EditSystemDialog::updateInfocardPreview()
{
    const QString xml = m_infocardEdit->toPlainText().trimmed();
    if (xml.isEmpty()) {
        m_infocardPreview->setText(tr("Keine Infocard gesetzt."));
        return;
    }

    QString error;
    if (!XmlInfocard::validate(xml, error)) {
        m_infocardPreview->setText(tr("Ungültige Infocard: %1").arg(error));
        return;
    }

    m_infocardPreview->setText(XmlInfocard::parseToPlainText(xml));
}

} // namespace flatlas::editors
