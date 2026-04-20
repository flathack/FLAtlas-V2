// tools/PathFinderDialog.cpp – Dialog zur Pfadfindung zwischen Systemen

#include "PathFinderDialog.h"
#include "ShortestPathGenerator.h"
#include "domain/UniverseData.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace flatlas::tools {

PathFinderDialog::PathFinderDialog(const flatlas::domain::UniverseData *universe,
                                   QWidget *parent)
    : QDialog(parent), m_universe(universe)
{
    setWindowTitle(tr("Shortest Path Finder"));
    setMinimumSize(450, 350);
    resize(500, 400);
    buildUi();
    populateSystems();
}

void PathFinderDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Input area
    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("From:")), 0, 0);
    m_fromCombo = new QComboBox;
    m_fromCombo->setEditable(true);
    m_fromCombo->setInsertPolicy(QComboBox::NoInsert);
    grid->addWidget(m_fromCombo, 0, 1);

    grid->addWidget(new QLabel(tr("To:")), 1, 0);
    m_toCombo = new QComboBox;
    m_toCombo->setEditable(true);
    m_toCombo->setInsertPolicy(QComboBox::NoInsert);
    grid->addWidget(m_toCombo, 1, 1);

    m_findButton = new QPushButton(tr("Find Shortest Path"));
    grid->addWidget(m_findButton, 2, 0, 1, 2);

    mainLayout->addLayout(grid);

    // Result area
    m_resultLabel = new QLabel;
    m_resultLabel->setWordWrap(true);
    mainLayout->addWidget(m_resultLabel);

    m_pathDisplay = new QTextEdit;
    m_pathDisplay->setReadOnly(true);
    mainLayout->addWidget(m_pathDisplay);

    connect(m_findButton, &QPushButton::clicked, this, &PathFinderDialog::onFindPath);
}

void PathFinderDialog::populateSystems()
{
    if (!m_universe) return;

    QStringList names;
    for (const auto &sys : m_universe->systems) {
        const QString label = sys.displayName.isEmpty()
            ? sys.nickname
            : QStringLiteral("%1 (%2)").arg(sys.displayName, sys.nickname);
        names.append(label);
    }
    names.sort();

    m_fromCombo->addItems(names);
    m_toCombo->addItems(names);

    if (m_toCombo->count() > 1)
        m_toCombo->setCurrentIndex(1);
}

void PathFinderDialog::onFindPath()
{
    if (!m_universe) return;

    // Nickname aus der Auswahl extrahieren
    auto extractNickname = [](const QString &text) -> QString {
        // Format: "DisplayName (Nickname)" oder einfach "Nickname"
        int parenStart = text.lastIndexOf(QLatin1Char('('));
        int parenEnd = text.lastIndexOf(QLatin1Char(')'));
        if (parenStart >= 0 && parenEnd > parenStart)
            return text.mid(parenStart + 1, parenEnd - parenStart - 1).trimmed();
        return text.trimmed();
    };

    const QString from = extractNickname(m_fromCombo->currentText());
    const QString to = extractNickname(m_toCombo->currentText());

    ShortestPathGenerator pathGen(m_universe);
    QStringList path = pathGen.findShortestPath(from, to);

    m_lastPath = path;

    if (path.isEmpty()) {
        m_resultLabel->setText(tr("<b>No route found</b> between %1 and %2.")
                                  .arg(from, to));
        m_pathDisplay->clear();
    } else {
        m_resultLabel->setText(tr("<b>Route found:</b> %1 jumps, distance: %2")
                                  .arg(pathGen.lastPathJumps())
                                  .arg(pathGen.lastPathDistance(), 0, 'f', 1));

        // Pfad anzeigen mit Systemnamen
        QStringList displayPath;
        for (int i = 0; i < path.size(); ++i) {
            const auto *sys = m_universe->findSystem(path[i]);
            QString name = sys && !sys->displayName.isEmpty()
                ? QStringLiteral("%1 (%2)").arg(sys->displayName, path[i])
                : path[i];
            displayPath.append(QStringLiteral("%1. %2").arg(i + 1).arg(name));
        }
        m_pathDisplay->setPlainText(displayPath.join(QLatin1Char('\n')));

        emit pathFound(path);
    }
}

} // namespace flatlas::tools
