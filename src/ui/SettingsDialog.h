#pragma once

#include <QDialog>
#include <QHash>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QTextEdit;

namespace flatlas::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    bool requiresPinnedToolRefresh() const;
    bool resetRequested() const;

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    void refreshConfigManager();
    void chooseConfigPath();
    void importConfig();
    void exportConfig();
    void createConfigBackup();
    void applyConfigJson();
    void refreshIdsTargetDllSettings();
    void chooseIdsTargetDll();
    void saveIdsTargetDllSettings();
    void resetIdsTargetDll();
    void migrateFlatlasIdsEntries();
    void updateThemeColorButton(const QString &key);
    void startSuiteDownload(const QString &key, const QString &name, const QString &repoApiUrl);
    void downloadReleaseAsset(const QString &name, const QUrl &url);
    void updateSuiteButtons();
    void openInstalledTool(const QString &key);
    void registerInstalledTool(const QString &key, const QString &name, const QString &installDir, const QString &exePath);
    QString toolsDirectory() const;

    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QCheckBox *m_updateCheckBox = nullptr;
    QCheckBox *m_restoreTabsCheckBox = nullptr;
    QHash<QString, QCheckBox *> m_toolChecks;
    QHash<QString, QPushButton *> m_themeColorButtons;
    QHash<QString, QPushButton *> m_suiteButtons;
    QLabel *m_suiteStatusLabel = nullptr;
    QLineEdit *m_configPathEdit = nullptr;
    QTextEdit *m_configJsonEdit = nullptr;
    QLabel *m_configStatusLabel = nullptr;
    QLineEdit *m_idsTargetDllEdit = nullptr;
    QLabel *m_idsTargetStatusLabel = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    bool m_pinnedToolsChanged = false;
    bool m_resetRequested = false;
};

} // namespace flatlas::ui
