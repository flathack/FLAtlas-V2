#pragma once
// tools/PathFinderDialog.h – Dialog zur Pfadfindung zwischen Systemen

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QTextEdit;

namespace flatlas::domain { class UniverseData; }

namespace flatlas::tools {

/// Dialog zur Berechnung des kürzesten Pfades zwischen zwei Systemen.
class PathFinderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PathFinderDialog(const flatlas::domain::UniverseData *universe,
                              QWidget *parent = nullptr);

    /// Gibt den zuletzt berechneten Pfad zurück (System-Nicknames).
    QStringList lastPath() const { return m_lastPath; }

signals:
    /// Wird gesendet, wenn ein Pfad berechnet wurde und visualisiert werden soll.
    void pathFound(const QStringList &path);

private:
    void buildUi();
    void populateSystems();
    void onFindPath();

    const flatlas::domain::UniverseData *m_universe = nullptr;
    QComboBox *m_fromCombo = nullptr;
    QComboBox *m_toCombo = nullptr;
    QPushButton *m_findButton = nullptr;
    QLabel *m_resultLabel = nullptr;
    QTextEdit *m_pathDisplay = nullptr;
    QStringList m_lastPath;
};

} // namespace flatlas::tools
