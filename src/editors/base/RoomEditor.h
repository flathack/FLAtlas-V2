#pragma once
// editors/base/RoomEditor.h – Räume einer Basis bearbeiten (Phase 10)

#include <QWidget>
#include "domain/BaseData.h"

class QListWidget;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace flatlas::editors {

class RoomEditor : public QWidget {
    Q_OBJECT
public:
    explicit RoomEditor(QWidget *parent = nullptr);

    void setRooms(const QVector<flatlas::domain::RoomData> &rooms);
    QVector<flatlas::domain::RoomData> rooms() const;

    void setBaseNickname(const QString &nickname);

signals:
    void roomsChanged();

private:
    void setupUi();
    void refreshList();
    void onAdd();
    void onRemove();
    void onSelectionChanged();

    QVector<flatlas::domain::RoomData> m_rooms;
    QString m_baseNickname;

    QListWidget *m_roomList = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
};

} // namespace flatlas::editors
