#include "ModSettingsPage.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextStream>
#include <QVBoxLayout>

using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

namespace flatlas::editors {
namespace {

QStringList csvParts(const QString &value)
{
    QStringList parts = value.split(QLatin1Char(','));
    for (QString &part : parts)
        part = part.trimmed();
    return parts;
}

int toInt(const QString &value, int fallback = 0)
{
    bool ok = false;
    const int parsed = value.trimmed().toInt(&ok);
    return ok ? parsed : fallback;
}

QString bribeWithPrice(const QString &value, int price)
{
    QStringList parts = csvParts(value);
    if (parts.size() < 3)
        return value;
    parts[1] = QString::number(price);
    return parts.join(QStringLiteral(", "));
}

}

ModSettingsPage::ModSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::contextChanged,
            this,
            [this](const QString &) { reload(); });
    reload();
}

void ModSettingsPage::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);

    auto *bribeGroup = new QGroupBox(tr("NPC Bribes"), this);
    auto *form = new QFormLayout(bribeGroup);
    m_bribePriceSpin = new QSpinBox(bribeGroup);
    m_bribePriceSpin->setRange(0, 999999999);
    m_bribePriceSpin->setSingleStep(1000);
    form->addRow(tr("Globaler Bribe-Preis:"), m_bribePriceSpin);
    auto *hint = new QLabel(tr("This value is used for all bribe lines in mbases.ini."), bribeGroup);
    hint->setWordWrap(true);
    form->addRow(QString(), hint);
    root->addWidget(bribeGroup);

    root->addStretch();

    auto *bottom = new QWidget(this);
    auto *bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_statusLabel = new QLabel(bottom);
    bottomLayout->addWidget(m_statusLabel, 1);
    m_saveButton = new QPushButton(tr("Speichern"), bottom);
    m_saveButton->setMinimumWidth(120);
    m_saveButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f8f4d; color: white; border: 1px solid #2fb365; padding: 6px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #24a95a; }"
        "QPushButton:pressed { background: #176d3a; }"));
    bottomLayout->addWidget(m_saveButton);
    root->addWidget(bottom);

    connect(m_saveButton, &QPushButton::clicked, this, &ModSettingsPage::save);
}

QString ModSettingsPage::mbasesPath() const
{
    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    if (gameRoot.trimmed().isEmpty())
        return {};
    QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        dataDir = QDir(gameRoot).absoluteFilePath(QStringLiteral("DATA"));
    return flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS/mbases.ini"));
}

int ModSettingsPage::detectedBribePrice() const
{
    const QString path = mbasesPath();
    if (path.isEmpty())
        return 10000;
    QHash<int, int> counts;
    const IniDocument doc = IniParser::parseFile(path);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("GF_NPC"), Qt::CaseInsensitive) != 0)
            continue;
        for (const QString &value : section.values(QStringLiteral("bribe"))) {
            const QStringList parts = csvParts(value);
            if (parts.size() > 1) {
                const int price = toInt(parts.at(1), 0);
                if (price > 0)
                    counts[price] += 1;
            }
        }
    }
    int bestPrice = 10000;
    int bestCount = -1;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.value() > bestCount) {
            bestPrice = it.key();
            bestCount = it.value();
        }
    }
    return bestPrice;
}

void ModSettingsPage::reload()
{
    const QString path = mbasesPath();
    const bool hasPath = !path.isEmpty();
    m_bribePriceSpin->setEnabled(hasPath);
    m_saveButton->setEnabled(hasPath);
    if (!hasPath) {
        m_statusLabel->setText(tr("No active mod installation with mbases.ini."));
        emit titleChanged(tr("Mod Settings"));
        return;
    }
    m_bribePriceSpin->setValue(detectedBribePrice());
    m_statusLabel->setText(tr("Geladen: %1").arg(path));
    emit titleChanged(tr("Mod Settings - %1").arg(QFileInfo(flatlas::core::EditingContext::instance().primaryGamePath()).fileName()));
}

bool ModSettingsPage::writeBribePrice(int price, QString *errorMessage)
{
    const QString path = mbasesPath();
    if (path.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("mbases.ini was not found.");
        return false;
    }

    IniDocument doc = IniParser::parseFile(path);
    for (IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("GF_NPC"), Qt::CaseInsensitive) != 0)
            continue;
        for (auto &entry : section.entries) {
            if (entry.first.compare(QStringLiteral("bribe"), Qt::CaseInsensitive) == 0)
                entry.second = bribeWithPrice(entry.second, price);
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = tr("mbases.ini could not be written: %1").arg(path);
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << IniParser::serialize(doc);
    return true;
}

void ModSettingsPage::save()
{
    QString error;
    if (!writeBribePrice(m_bribePriceSpin->value(), &error)) {
        QMessageBox::warning(this, tr("Mod Settings"), error);
        return;
    }
    m_statusLabel->setText(tr("Bribe price saved: %1").arg(m_bribePriceSpin->value()));
}

} // namespace flatlas::editors
