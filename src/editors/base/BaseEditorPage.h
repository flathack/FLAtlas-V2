#pragma once
// editors/base/BaseEditorPage.h – Basis-Editor (Phase 10)

#include <QWidget>
#include <memory>
#include "domain/BaseData.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QSplitter;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;

namespace flatlas::editors {

class RoomEditor;

class BaseEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit BaseEditorPage(QWidget *parent = nullptr);
    ~BaseEditorPage() override;

    /// Load a base from INI file by nickname.
    bool loadBase(const QString &filePath, const QString &baseNickname);

    /// Load directly from a BaseData object (e.g. from builder).
    void setBase(std::unique_ptr<flatlas::domain::BaseData> base);

    /// Save the base to the given file path.
    bool save(const QString &filePath);

    flatlas::domain::BaseData *data() const;
    QString baseNickname() const;

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupToolBar();
    void populateFromData();
    void applyToData();
    void onNewBase();

    std::unique_ptr<flatlas::domain::BaseData> m_data;
    QString m_filePath;

    QToolBar *m_toolBar = nullptr;
    QSplitter *m_splitter = nullptr;

    // Properties panel
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_archetypeEdit = nullptr;
    QLineEdit *m_systemEdit = nullptr;
    QSpinBox *m_idsNameSpin = nullptr;
    QSpinBox *m_idsInfoSpin = nullptr;
    QLineEdit *m_posEdit = nullptr;
    QLineEdit *m_dockWithEdit = nullptr;

    // Room editor
    RoomEditor *m_roomEditor = nullptr;
};

} // namespace flatlas::editors
