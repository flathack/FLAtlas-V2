// editors/base/RoomEditor.cpp – Räume einer Basis bearbeiten (Phase 10)

#include "RoomEditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

using namespace flatlas::domain;

namespace flatlas::editors {

static const QStringList ROOM_TYPES = {
    QStringLiteral("Bar"),
    QStringLiteral("Equip"),
    QStringLiteral("Commodity"),
    QStringLiteral("ShipDealer"),
    QStringLiteral("Trader"),
    QStringLiteral("Deck"),
};

RoomEditor::RoomEditor(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void RoomEditor::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *group = new QGroupBox(tr("Rooms"), this);
    auto *groupLayout = new QVBoxLayout(group);

    // Room list
    m_roomList = new QListWidget(group);
    m_roomList->setAlternatingRowColors(true);
    connect(m_roomList, &QListWidget::currentRowChanged,
            this, [this](int) { onSelectionChanged(); });
    groupLayout->addWidget(m_roomList);

    // Add controls
    auto *addRow = new QHBoxLayout();
    m_typeCombo = new QComboBox(group);
    m_typeCombo->addItems(ROOM_TYPES);
    m_typeCombo->setEditable(true);
    addRow->addWidget(new QLabel(tr("Type:"), group));
    addRow->addWidget(m_typeCombo);

    m_nicknameEdit = new QLineEdit(group);
    m_nicknameEdit->setPlaceholderText(tr("Room nickname (auto if empty)"));
    addRow->addWidget(m_nicknameEdit);

    m_addBtn = new QPushButton(tr("Add"), group);
    connect(m_addBtn, &QPushButton::clicked, this, &RoomEditor::onAdd);
    addRow->addWidget(m_addBtn);

    m_removeBtn = new QPushButton(tr("Remove"), group);
    m_removeBtn->setEnabled(false);
    connect(m_removeBtn, &QPushButton::clicked, this, &RoomEditor::onRemove);
    addRow->addWidget(m_removeBtn);

    groupLayout->addLayout(addRow);
    mainLayout->addWidget(group);
}

void RoomEditor::setRooms(const QVector<RoomData> &rooms)
{
    m_rooms = rooms;
    refreshList();
}

QVector<RoomData> RoomEditor::rooms() const
{
    return m_rooms;
}

void RoomEditor::setBaseNickname(const QString &nickname)
{
    m_baseNickname = nickname;
}

void RoomEditor::refreshList()
{
    m_roomList->clear();
    for (const auto &room : m_rooms) {
        m_roomList->addItem(QStringLiteral("%1 [%2]").arg(room.nickname, room.type));
    }
    m_removeBtn->setEnabled(false);
}

void RoomEditor::onAdd()
{
    RoomData room;
    room.type = m_typeCombo->currentText().trimmed();
    if (room.type.isEmpty()) return;

    QString nick = m_nicknameEdit->text().trimmed();
    if (nick.isEmpty()) {
        // Auto-generate nickname: base_type
        nick = m_baseNickname.isEmpty()
            ? room.type.toLower()
            : m_baseNickname + QStringLiteral("_") + room.type.toLower();
    }
    room.nickname = nick;

    m_rooms.append(room);
    refreshList();
    m_nicknameEdit->clear();
    emit roomsChanged();
}

void RoomEditor::onRemove()
{
    int row = m_roomList->currentRow();
    if (row < 0 || row >= m_rooms.size()) return;

    m_rooms.removeAt(row);
    refreshList();
    emit roomsChanged();
}

void RoomEditor::onSelectionChanged()
{
    m_removeBtn->setEnabled(m_roomList->currentRow() >= 0);
}

} // namespace flatlas::editors
