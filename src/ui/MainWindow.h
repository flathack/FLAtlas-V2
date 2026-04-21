#pragma once

#include <QMainWindow>

class QSplitter;
class QLabel;
class QProgressBar;
class QTabBar;
class QStackedWidget;
class QPushButton;
namespace flatlas::ui { class CenterTabWidget; }
namespace flatlas::ui { class PropertiesPanel; }
namespace flatlas::editors { class SystemEditorPage; }
namespace flatlas::editors { class UniverseEditorPage; }
namespace flatlas::editors { class BaseEditorPage; }
namespace flatlas::editors { class TradeRoutePage; }
namespace flatlas::editors { class IdsEditorPage; }
namespace flatlas::editors { class ModManagerPage; }
namespace flatlas::editors { class NpcEditorPage; }
namespace flatlas::tools { class HelpBrowser; }
namespace flatlas::rendering { class ModelViewerPage; }

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
    void applyThemeStyling();
    void openSystemFile();
    void openIniFile();
    void openUniverseFile();
    void openBaseEditor();
    void openTradeRoutes();
    void openIdsEditor();
    void openModManager();
    void openNpcEditor();
    void openInfocardEditor();
    void openNewsRumorEditor();
    void openModelViewer();
    void saveCurrentSystem();
    void saveCurrentFile();
    void newSystem();
    void openSystemFromUniverse(const QString &nickname,
                                const QString &systemFile,
                                const QString &ingameName);
    void openUniverseFromContext();
    void handleEditingContextChanged();
    void closeContextBoundTabs();
    bool closeTabWithPrompt(int index, bool force = false);
    bool confirmCloseDirtyWidget(QWidget *widget, const QString &titleForUser);
    bool saveWidgetWithPrompt(QWidget *widget);
    bool isWidgetDirty(QWidget *widget) const;
    QString tabTitleForWidget(QWidget *widget) const;
    void showContextHelp();
    void launchFreelancerFromContext();
    flatlas::editors::SystemEditorPage *currentSystemEditor() const;
    static QString formatSystemTabTitle(const QString &editorTitle, const QString &ingameName);

    QSplitter *m_mainSplitter = nullptr;
    flatlas::ui::CenterTabWidget *m_centerTabs = nullptr;
    flatlas::ui::PropertiesPanel *m_propertiesPanel = nullptr;
    flatlas::tools::HelpBrowser *m_helpBrowser = nullptr;
    QLabel *m_editingLabel = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
};
