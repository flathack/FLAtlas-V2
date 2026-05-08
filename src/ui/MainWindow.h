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
namespace flatlas::editors { class TradeRoutePage; }
namespace flatlas::editors { class IdsEditorPage; }
namespace flatlas::editors { class ModManagerPage; }
namespace flatlas::editors { class ModSettingsPage; }
namespace flatlas::editors { class NpcEditorPage; }
namespace flatlas::editors { class FactionEditorPage; }
namespace flatlas::editors { class NewsRumorEditor; }
namespace flatlas::tools { class HelpBrowser; }
namespace flatlas::tools { class KeyboardShortcutOverviewDialog; }
namespace flatlas::rendering { class ModelViewerPage; }

/// FLAtlas-Hauptfenster – schlanke Orchestrierung, delegiert an Panels und Editoren.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool showModelInViewer(const QString &modelPath, const QString &displayLabel = QString());

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createMenus();
    void createPanels();
    void createStatusBar();
    void restoreWindowState();
    void saveWindowState();
    void applyThemeStyling();
    void openIniFile();
    void openIniFile(const QString &filePath, const QString &searchText = QString(), int lineNumber = 0);
    void openTradeRoutes();
    void openIdsEditor();
    void openModManager();
    void openModSettings();
    void openNpcEditor();
    void openFactionEditor();
    void openNewsRumorEditor();
    void openModelViewer();
    void openActivityLog();
    void connectNpcEditorPage(flatlas::editors::NpcEditorPage *editor);
    void connectFactionEditorPage(flatlas::editors::FactionEditorPage *editor);
    void connectNewsRumorEditor(flatlas::editors::NewsRumorEditor *editor);
    void connectModelViewerPage(flatlas::rendering::ModelViewerPage *page);
    void openSettingsDialog();
    void applyPinnedToolSettings();
    void restoreOpenToolTabs();
    void saveOpenToolTabs();
    bool openToolByKey(const QString &key, bool pinned);
    void saveCurrentSystem();
    void saveCurrentFile();
    void openSystemFromUniverse(const QString &nickname,
                                const QString &systemFile,
                                const QString &ingameName);
    void open3DSystemEditorFor(flatlas::editors::SystemEditorPage *editor);
    void openUniverseFromContext();
    void handleEditingContextChanged();
    void closeContextBoundTabs();
    bool closeTabWithPrompt(int index, bool force = false);
    bool confirmCloseDirtyWidget(QWidget *widget, const QString &titleForUser);
    bool saveWidgetWithPrompt(QWidget *widget);
    bool isWidgetDirty(QWidget *widget) const;
    QString tabTitleForWidget(QWidget *widget) const;
    void showContextHelp();
    void showShortcutOverview();
    void launchFreelancerFromContext();
    flatlas::editors::SystemEditorPage *currentSystemEditor() const;
    static QString formatSystemTabTitle(const QString &editorTitle, const QString &ingameName);
    flatlas::rendering::ModelViewerPage *ensureModelViewerPage();

    QSplitter *m_mainSplitter = nullptr;
    flatlas::ui::CenterTabWidget *m_centerTabs = nullptr;
    flatlas::ui::PropertiesPanel *m_propertiesPanel = nullptr;
    flatlas::tools::HelpBrowser *m_helpBrowser = nullptr;
    flatlas::tools::KeyboardShortcutOverviewDialog *m_shortcutOverviewDialog = nullptr;
    QLabel *m_editingLabel = nullptr;
    QLabel *m_progressPercentLabel = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    bool m_openToolTabsRestored = false;
    bool m_suppressTabStateSave = true;
};
