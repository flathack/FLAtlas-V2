#pragma once
// editors/npc/NpcEditorPage.h – NPC-Editor (Phase 14)

#include <QWidget>
#include <QVector>
#include "MbaseOperations.h"

class QToolBar;
class QComboBox;
class QTableWidget;
class QLabel;

namespace flatlas::editors {

class NpcEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit NpcEditorPage(QWidget *parent = nullptr);

    /// Load an MBases.ini file.
    void loadFile(const QString &filePath);

    /// Save back to file.
    bool saveFile(const QString &filePath);

    /// Get loaded base count.
    int baseCount() const { return m_bases.size(); }

    /// Get total NPC count.
    int npcCount() const;

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupToolBar();
    void refreshBaseSelector();
    void refreshNpcTable();
    void onBaseSelected(int index);
    void onLoadClicked();
    void onSaveClicked();

    QString m_filePath;
    QVector<MbaseData> m_bases;
    int m_currentBaseIndex = -1;

    QToolBar *m_toolBar = nullptr;
    QComboBox *m_baseSelector = nullptr;
    QTableWidget *m_npcTable = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace flatlas::editors
