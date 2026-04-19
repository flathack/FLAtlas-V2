#pragma once

#include <QMainWindow>

class QSplitter;
namespace flatlas::ui { class BrowserPanel; }
namespace flatlas::ui { class CenterTabWidget; }
namespace flatlas::ui { class PropertiesPanel; }
namespace flatlas::editors { class SystemEditorPage; }

/// FLAtlas-Hauptfenster – schlanke Orchestrierung, delegiert an Panels und Editoren.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createMenus();
    void createPanels();
    void createStatusBar();
    void restoreWindowState();
    void saveWindowState();
    void openSystemFile();
    void saveCurrentSystem();
    void newSystem();
    flatlas::editors::SystemEditorPage *currentSystemEditor() const;

    QSplitter *m_mainSplitter = nullptr;
    flatlas::ui::BrowserPanel *m_browserPanel = nullptr;
    flatlas::ui::CenterTabWidget *m_centerTabs = nullptr;
    flatlas::ui::PropertiesPanel *m_propertiesPanel = nullptr;
};
