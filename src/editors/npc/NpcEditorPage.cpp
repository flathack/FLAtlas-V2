#include "NpcEditorPage.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/io/DllResources.h"
#include "infrastructure/parser/IniParser.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using flatlas::infrastructure::IdsStringTable;
using flatlas::infrastructure::IdsDataService;
using flatlas::infrastructure::IdsDataset;
using flatlas::infrastructure::DllResources;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

namespace flatlas::editors {
namespace {

QString keyOf(const QString &value)
{
    return value.trimmed().toLower();
}

QString dataDirForGameRoot(const QString &gameRoot)
{
    QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        dataDir = QDir(gameRoot).absoluteFilePath(QStringLiteral("DATA"));
    return dataDir;
}

QString exeDirForGameRoot(const QString &gameRoot)
{
    QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    if (exeDir.isEmpty())
        exeDir = QDir(gameRoot).absoluteFilePath(QStringLiteral("EXE"));
    return exeDir;
}

QStringList csvParts(const QString &value)
{
    QStringList parts = value.split(QLatin1Char(','));
    for (QString &part : parts)
        part = part.trimmed();
    return parts;
}

int toInt(const QString &value, int fallback = 0)
{
    bool ok = false;
    const int parsed = value.trimmed().toInt(&ok);
    return ok ? parsed : fallback;
}

void appendUnique(QStringList *list, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return;
    for (const QString &existing : std::as_const(*list)) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0)
            return;
    }
    list->append(trimmed);
}

QString sectionValue(const IniSection &section, const QString &key)
{
    return section.value(key).trimmed();
}

bool isKnownNpcKey(const QString &key)
{
    static const QSet<QString> known = {
        QStringLiteral("nickname"),
        QStringLiteral("body"),
        QStringLiteral("head"),
        QStringLiteral("lefthand"),
        QStringLiteral("righthand"),
        QStringLiteral("individual_name"),
        QStringLiteral("info"),
        QStringLiteral("affiliation"),
        QStringLiteral("voice"),
        QStringLiteral("npc_type"),
        QStringLiteral("room"),
        QStringLiteral("bribe"),
        QStringLiteral("rumor"),
        QStringLiteral("rumor_type2")
    };
    return known.contains(keyOf(key));
}

bool bribesEqual(const QVector<NpcBribe> &left, const QVector<NpcBribe> &right)
{
    if (left.size() != right.size())
        return false;
    for (int i = 0; i < left.size(); ++i) {
        if (left.at(i).faction != right.at(i).faction
            || left.at(i).price != right.at(i).price
            || left.at(i).ids != right.at(i).ids) {
            return false;
        }
    }
    return true;
}

bool rumorsEqual(const QVector<NpcRumorAssignment> &left, const QVector<NpcRumorAssignment> &right)
{
    if (left.size() != right.size())
        return false;
    for (int i = 0; i < left.size(); ++i) {
        if (left.at(i).kind != right.at(i).kind
            || left.at(i).stateFrom != right.at(i).stateFrom
            || left.at(i).stateTo != right.at(i).stateTo
            || left.at(i).weight != right.at(i).weight
            || left.at(i).ids != right.at(i).ids) {
            return false;
        }
    }
    return true;
}

bool npcEditableFieldsEqual(const NpcRecord &left, const NpcRecord &right)
{
    return left.nickname == right.nickname
        && left.room == right.room
        && left.baseFaction == right.baseFaction
        && left.affiliation == right.affiliation
        && left.npcType == right.npcType
        && left.body == right.body
        && left.head == right.head
        && left.leftHand == right.leftHand
        && left.rightHand == right.rightHand
        && left.voice == right.voice
        && left.individualName == right.individualName
        && left.individualNameText == right.individualNameText
        && left.info == right.info
        && bribesEqual(left.bribes, right.bribes)
        && rumorsEqual(left.rumors, right.rumors);
}

NpcBribe parseBribe(const QString &value)
{
    const QStringList parts = csvParts(value);
    NpcBribe bribe;
    if (!parts.isEmpty())
        bribe.faction = parts.at(0);
    if (parts.size() > 1)
        bribe.price = toInt(parts.at(1));
    if (parts.size() > 2)
        bribe.ids = toInt(parts.at(2));
    return bribe;
}

NpcRumorAssignment parseRumor(const QString &kind, const QString &value)
{
    const QStringList parts = csvParts(value);
    NpcRumorAssignment rumor;
    rumor.kind = kind;
    if (!parts.isEmpty())
        rumor.stateFrom = parts.at(0);
    if (parts.size() > 1)
        rumor.stateTo = parts.at(1);
    if (parts.size() > 2)
        rumor.weight = toInt(parts.at(2), 1);
    if (parts.size() > 3)
        rumor.ids = toInt(parts.at(3));
    return rumor;
}

QString bribeValueWithPrice(const NpcBribe &bribe, int price)
{
    return QStringLiteral("%1, %2, %3").arg(bribe.faction, QString::number(price), QString::number(bribe.ids));
}

QString rumorValue(const NpcRumorAssignment &rumor)
{
    return QStringLiteral("%1, %2, %3, %4")
        .arg(rumor.stateFrom, rumor.stateTo, QString::number(rumor.weight), QString::number(rumor.ids));
}

int mbaseEnd(const IniDocument &doc, int start)
{
    int end = doc.size();
    for (int i = start + 1; i < doc.size(); ++i) {
        if (doc.at(i).name.compare(QStringLiteral("MBase"), Qt::CaseInsensitive) == 0) {
            end = i;
            break;
        }
    }
    return end;
}

QVector<int> sectionIndexesForMbase(const IniDocument &doc, const QString &baseNickname)
{
    QVector<int> indexes;
    for (int i = 0; i < doc.size(); ++i) {
        const IniSection &section = doc.at(i);
        if (section.name.compare(QStringLiteral("MBase"), Qt::CaseInsensitive) != 0)
            continue;
        if (section.value(QStringLiteral("nickname")).trimmed().compare(baseNickname, Qt::CaseInsensitive) != 0)
            continue;
        const int end = mbaseEnd(doc, i);
        for (int j = i; j < end; ++j)
            indexes.append(j);
        break;
    }
    return indexes;
}

IniSection npcToSection(const NpcRecord &npc, int bribePrice)
{
    IniSection section;
    section.name = QStringLiteral("GF_NPC");
    section.entries.append({QStringLiteral("nickname"), npc.nickname.trimmed()});
    if (!npc.body.trimmed().isEmpty())
        section.entries.append({QStringLiteral("body"), npc.body.trimmed()});
    if (!npc.head.trimmed().isEmpty())
        section.entries.append({QStringLiteral("head"), npc.head.trimmed()});
    if (!npc.leftHand.trimmed().isEmpty())
        section.entries.append({QStringLiteral("lefthand"), npc.leftHand.trimmed()});
    if (!npc.rightHand.trimmed().isEmpty())
        section.entries.append({QStringLiteral("righthand"), npc.rightHand.trimmed()});
    if (npc.individualName > 0)
        section.entries.append({QStringLiteral("individual_name"), QString::number(npc.individualName)});
    if (npc.info > 0)
        section.entries.append({QStringLiteral("info"), QString::number(npc.info)});
    if (!npc.affiliation.trimmed().isEmpty())
        section.entries.append({QStringLiteral("affiliation"), npc.affiliation.trimmed()});
    if (!npc.voice.trimmed().isEmpty())
        section.entries.append({QStringLiteral("voice"), npc.voice.trimmed()});
    if (!npc.npcType.trimmed().isEmpty())
        section.entries.append({QStringLiteral("npc_type"), npc.npcType.trimmed()});
    if (!npc.room.trimmed().isEmpty())
        section.entries.append({QStringLiteral("room"), npc.room.trimmed()});
    for (const auto &entry : npc.preservedEntries) {
        if (!isKnownNpcKey(entry.first))
            section.entries.append(entry);
    }
    for (const NpcBribe &bribe : npc.bribes) {
        if (!bribe.faction.trimmed().isEmpty())
            section.entries.append({QStringLiteral("bribe"), bribeValueWithPrice(bribe, bribePrice)});
    }
    for (const NpcRumorAssignment &rumor : npc.rumors) {
        if (rumor.ids > 0 && !rumor.kind.trimmed().isEmpty())
            section.entries.append({rumor.kind.trimmed(), rumorValue(rumor)});
    }
    return section;
}

bool npcValueForKnownKey(const NpcRecord &npc, const QString &key, int bribePrice, QStringList *values)
{
    if (!values)
        return false;
    values->clear();
    const QString normalized = keyOf(key);
    if (normalized == QStringLiteral("nickname")) {
        values->append(npc.nickname.trimmed());
    } else if (normalized == QStringLiteral("body")) {
        if (!npc.body.trimmed().isEmpty())
            values->append(npc.body.trimmed());
    } else if (normalized == QStringLiteral("head")) {
        if (!npc.head.trimmed().isEmpty())
            values->append(npc.head.trimmed());
    } else if (normalized == QStringLiteral("lefthand")) {
        if (!npc.leftHand.trimmed().isEmpty())
            values->append(npc.leftHand.trimmed());
    } else if (normalized == QStringLiteral("righthand")) {
        if (!npc.rightHand.trimmed().isEmpty())
            values->append(npc.rightHand.trimmed());
    } else if (normalized == QStringLiteral("individual_name")) {
        if (npc.individualName > 0)
            values->append(QString::number(npc.individualName));
    } else if (normalized == QStringLiteral("info")) {
        if (npc.info > 0)
            values->append(QString::number(npc.info));
    } else if (normalized == QStringLiteral("affiliation")) {
        if (!npc.affiliation.trimmed().isEmpty())
            values->append(npc.affiliation.trimmed());
    } else if (normalized == QStringLiteral("voice")) {
        if (!npc.voice.trimmed().isEmpty())
            values->append(npc.voice.trimmed());
    } else if (normalized == QStringLiteral("npc_type")) {
        if (!npc.npcType.trimmed().isEmpty())
            values->append(npc.npcType.trimmed());
    } else if (normalized == QStringLiteral("room")) {
        if (!npc.room.trimmed().isEmpty())
            values->append(npc.room.trimmed());
    } else if (normalized == QStringLiteral("bribe")) {
        for (const NpcBribe &bribe : npc.bribes) {
            if (!bribe.faction.trimmed().isEmpty())
                values->append(bribeValueWithPrice(bribe, bribePrice));
        }
    } else if (normalized == QStringLiteral("rumor") || normalized == QStringLiteral("rumor_type2")) {
        for (const NpcRumorAssignment &rumor : npc.rumors) {
            if (keyOf(rumor.kind) == normalized && rumor.ids > 0)
                values->append(rumorValue(rumor));
        }
    } else {
        return false;
    }
    return true;
}

IniSection updateNpcSectionPreservingOrder(const IniSection &oldSection,
                                           const NpcRecord &npc,
                                           int bribePrice)
{
    IniSection updated = oldSection;
    updated.entries.clear();

    QSet<QString> seenKeys;
    for (const auto &entry : oldSection.entries) {
        const QString normalized = keyOf(entry.first);
        QStringList values;
        if (!npcValueForKnownKey(npc, entry.first, bribePrice, &values)) {
            updated.entries.append(entry);
            continue;
        }
        if (seenKeys.contains(normalized))
            continue;
        seenKeys.insert(normalized);
        for (const QString &value : values)
            updated.entries.append({entry.first, value});
    }

    const IniSection canonical = npcToSection(npc, bribePrice);
    for (const auto &entry : canonical.entries) {
        if (!isKnownNpcKey(entry.first))
            continue;
        if (!seenKeys.contains(keyOf(entry.first))) {
            updated.entries.append(entry);
            seenKeys.insert(keyOf(entry.first));
        }
    }
    return updated;
}

QString sectionLabel(const IniSection &section)
{
    return section.name.trimmed().toLower();
}

const NpcRecord *findNpcByKey(const NpcBaseRecord &base, const QString &npcKey)
{
    for (const NpcRecord &npc : base.npcs) {
        if (keyOf(npc.nickname) == npcKey)
            return &npc;
    }
    return nullptr;
}

QHash<QString, QStringList> npcNamesByFaction(const NpcBaseRecord &base)
{
    QHash<QString, QStringList> result;
    for (const NpcRecord &npc : base.npcs) {
        if (!npc.baseFaction.trimmed().isEmpty())
            appendUnique(&result[npc.baseFaction.trimmed()], npc.nickname);
    }
    return result;
}

IniSection updateBaseFactionSectionPreservingOrder(const IniSection &section,
                                                   const QStringList &desiredNpcs)
{
    IniSection updated = section;
    updated.entries.clear();

    QStringList written;
    for (const auto &entry : section.entries) {
        if (entry.first.compare(QStringLiteral("npc"), Qt::CaseInsensitive) != 0) {
            updated.entries.append(entry);
            continue;
        }
        for (const QString &npcName : desiredNpcs) {
            if (npcName.compare(entry.second.trimmed(), Qt::CaseInsensitive) == 0) {
                updated.entries.append({entry.first, npcName});
                appendUnique(&written, npcName);
                break;
            }
        }
    }

    for (const QString &npcName : desiredNpcs) {
        if (!written.contains(npcName, Qt::CaseInsensitive))
            updated.entries.append({QStringLiteral("npc"), npcName});
    }
    return updated;
}

IniDocument rebuildMbaseBlockPreservingOrder(const IniDocument &doc,
                                             const QVector<int> &oldIndexes,
                                             const NpcBaseRecord &base,
                                             int bribePrice)
{
    IniDocument block;
    const QHash<QString, QStringList> npcsByFaction = npcNamesByFaction(base);
    QSet<QString> writtenFactionKeys;
    QSet<QString> writtenNpcKeys;
    int insertNewFactionsAfter = -1;
    int insertNewNpcsBefore = -1;

    for (int sourcePos = 0; sourcePos < oldIndexes.size(); ++sourcePos) {
        const IniSection &section = doc.at(oldIndexes.at(sourcePos));
        const QString label = sectionLabel(section);

        if (label == QStringLiteral("basefaction")) {
            const QString faction = sectionValue(section, QStringLiteral("faction"));
            writtenFactionKeys.insert(keyOf(faction));
            block.append(updateBaseFactionSectionPreservingOrder(section, npcsByFaction.value(faction)));
            insertNewFactionsAfter = block.size() - 1;
            continue;
        }

        if (label == QStringLiteral("gf_npc")) {
            const QString npcKey = keyOf(sectionValue(section, QStringLiteral("nickname")));
            const NpcRecord *npc = findNpcByKey(base, npcKey);
            if (!npc)
                continue;
            writtenNpcKeys.insert(keyOf(npc->nickname));
            block.append(npc->dirty ? updateNpcSectionPreservingOrder(section, *npc, bribePrice) : section);
            continue;
        }

        if (label == QStringLiteral("mroom") && insertNewNpcsBefore < 0)
            insertNewNpcsBefore = block.size();
        block.append(section);
    }

    IniDocument newFactionSections;
    for (auto it = npcsByFaction.constBegin(); it != npcsByFaction.constEnd(); ++it) {
        if (writtenFactionKeys.contains(keyOf(it.key())))
            continue;
        IniSection faction;
        faction.name = QStringLiteral("BaseFaction");
        faction.entries.append({QStringLiteral("faction"), it.key()});
        for (const QString &npcName : it.value())
            faction.entries.append({QStringLiteral("npc"), npcName});
        newFactionSections.append(faction);
    }
    if (!newFactionSections.isEmpty()) {
        const int insertAt = insertNewFactionsAfter >= 0 ? insertNewFactionsAfter + 1 : qMin(1, block.size());
        for (int i = 0; i < newFactionSections.size(); ++i)
            block.insert(insertAt + i, newFactionSections.at(i));
        if (insertNewNpcsBefore >= insertAt)
            insertNewNpcsBefore += newFactionSections.size();
    }

    IniDocument newNpcSections;
    for (const NpcRecord &npc : base.npcs) {
        if (!writtenNpcKeys.contains(keyOf(npc.nickname)))
            newNpcSections.append(npcToSection(npc, bribePrice));
    }
    if (!newNpcSections.isEmpty()) {
        const int insertAt = insertNewNpcsBefore >= 0 ? insertNewNpcsBefore : block.size();
        for (int i = 0; i < newNpcSections.size(); ++i)
            block.insert(insertAt + i, newNpcSections.at(i));
    }

    return block;
}

void setComboText(QComboBox *combo, const QString &text)
{
    if (!combo)
        return;
    const int index = combo->findText(text, Qt::MatchFixedString);
    if (index >= 0)
        combo->setCurrentIndex(index);
    else
        combo->setCurrentText(text);
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QString canonicalRoomName(const NpcBaseRecord &base, const QString &roomName)
{
    const QString clean = roomName.trimmed();
    if (clean.isEmpty())
        return {};
    for (const NpcRoomRecord &room : base.rooms) {
        if (room.nickname.compare(clean, Qt::CaseInsensitive) == 0)
            return room.nickname;
    }
    return clean;
}

}

NpcEditorPage::NpcEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::contextChanged,
            this,
            [this](const QString &) { scheduleReloadCurrentContext(); });
    scheduleReloadCurrentContext();
}

int NpcEditorPage::baseCount() const
{
    return m_bases.size();
}

int NpcEditorPage::npcCount() const
{
    int count = 0;
    for (const NpcBaseRecord &base : m_bases)
        count += base.npcs.size();
    return count;
}

void NpcEditorPage::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    setupToolBar();
    root->addWidget(m_toolBar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    root->addWidget(splitter, 1);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->addWidget(new QLabel(tr("Rooms"), left));
    m_roomList = new QListWidget(left);
    leftLayout->addWidget(m_roomList, 1);
    connect(m_roomList, &QListWidget::currentRowChanged, this, &NpcEditorPage::onRoomChanged);

    auto *middle = new QWidget(splitter);
    auto *middleLayout = new QVBoxLayout(middle);
    middleLayout->setContentsMargins(8, 8, 8, 8);
    middleLayout->addWidget(new QLabel(tr("NPCs in Room"), middle));
    m_npcTable = new QTableWidget(middle);
    m_npcTable->setColumnCount(6);
    m_npcTable->setHorizontalHeaderLabels({tr("Nickname"), tr("Name"), tr("Faction"), tr("Affiliation"), tr("Bribes"), tr("Rumors")});
    m_npcTable->horizontalHeader()->setStretchLastSection(false);
    m_npcTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_npcTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_npcTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_npcTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_npcTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_npcTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_npcTable->verticalHeader()->setVisible(false);
    m_npcTable->setAlternatingRowColors(true);
    m_npcTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_npcTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_npcTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    middleLayout->addWidget(m_npcTable, 1);
    connect(m_npcTable, &QTableWidget::currentCellChanged, this, &NpcEditorPage::onNpcSelectionChanged);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    auto *tabs = new QTabWidget(right);
    rightLayout->addWidget(tabs, 1);

    auto *generalTab = new QWidget(tabs);
    auto *generalLayout = new QFormLayout(generalTab);
    m_nicknameEdit = new QLineEdit(generalTab);
    m_roomCombo = new QComboBox(generalTab);
    m_baseFactionCombo = new QComboBox(generalTab);
    m_affiliationCombo = new QComboBox(generalTab);
    m_bodyCombo = new QComboBox(generalTab);
    m_headCombo = new QComboBox(generalTab);
    m_leftHandCombo = new QComboBox(generalTab);
    m_rightHandCombo = new QComboBox(generalTab);
    m_voiceCombo = new QComboBox(generalTab);
    for (QComboBox *combo : {m_roomCombo, m_baseFactionCombo, m_affiliationCombo, m_bodyCombo, m_headCombo, m_leftHandCombo, m_rightHandCombo, m_voiceCombo})
        combo->setEditable(true);
    m_individualNameSpin = new QSpinBox(generalTab);
    m_individualNameTextEdit = new QLineEdit(generalTab);
    m_infoSpin = new QSpinBox(generalTab);
    m_individualNameSpin->setRange(0, 999999999);
    m_infoSpin->setRange(0, 999999999);
    m_namePreviewLabel = new QLabel(generalTab);
    m_namePreviewLabel->setWordWrap(true);
    m_infoPreviewLabel = new QLabel(generalTab);
    m_infoPreviewLabel->setWordWrap(true);
    generalLayout->addRow(tr("Nickname:"), m_nicknameEdit);
    generalLayout->addRow(tr("Room:"), m_roomCombo);
    generalLayout->addRow(tr("Base Faction:"), m_baseFactionCombo);
    generalLayout->addRow(tr("Affiliation:"), m_affiliationCombo);
    generalLayout->addRow(tr("Body:"), m_bodyCombo);
    generalLayout->addRow(tr("Head:"), m_headCombo);
    generalLayout->addRow(tr("Left hand:"), m_leftHandCombo);
    generalLayout->addRow(tr("Right hand:"), m_rightHandCombo);
    generalLayout->addRow(tr("Voice:"), m_voiceCombo);
    generalLayout->addRow(tr("IDS Name ID:"), m_individualNameSpin);
    generalLayout->addRow(tr("NPC Name:"), m_individualNameTextEdit);
    generalLayout->addRow(tr("IDS Info:"), m_infoSpin);
    generalLayout->addRow(tr("Info Preview:"), m_infoPreviewLabel);
    tabs->addTab(generalTab, tr("NPC"));

    auto *bribeTab = new QWidget(tabs);
    auto *bribeLayout = new QVBoxLayout(bribeTab);
    m_bribeTable = new QTableWidget(bribeTab);
    m_bribeTable->setColumnCount(2);
    m_bribeTable->setHorizontalHeaderLabels({tr("Ingame Name"), tr("Nickname")});
    m_bribeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_bribeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_bribeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    bribeLayout->addWidget(m_bribeTable, 1);
    m_bribeTextPreview = new QPlainTextEdit(bribeTab);
    m_bribeTextPreview->setReadOnly(true);
    m_bribeTextPreview->setMinimumHeight(120);
    m_bribeTextPreview->setPlaceholderText(tr("Select a bribe to see the ingame text."));
    bribeLayout->addWidget(m_bribeTextPreview, 0);
    auto *bribeButtons = new QWidget(bribeTab);
    auto *bribeButtonLayout = new QHBoxLayout(bribeButtons);
    bribeButtonLayout->setContentsMargins(0, 0, 0, 0);
    auto *addBribe = new QPushButton(tr("Add Bribe"), bribeButtons);
    auto *removeBribe = new QPushButton(tr("Remove Bribe"), bribeButtons);
    bribeButtonLayout->addWidget(addBribe);
    bribeButtonLayout->addWidget(removeBribe);
    bribeButtonLayout->addStretch();
    bribeLayout->addWidget(bribeButtons);
    connect(addBribe, &QPushButton::clicked, this, &NpcEditorPage::onAddBribe);
    connect(removeBribe, &QPushButton::clicked, this, &NpcEditorPage::onRemoveBribe);
    connect(m_bribeTable, &QTableWidget::currentCellChanged, this, [this]() { refreshSelectedBribeText(); });
    tabs->addTab(bribeTab, tr("Bribes"));

    auto *rumorTab = new QWidget(tabs);
    auto *rumorLayout = new QVBoxLayout(rumorTab);
    auto *rumorPicker = new QWidget(rumorTab);
    auto *rumorPickerLayout = new QHBoxLayout(rumorPicker);
    rumorPickerLayout->setContentsMargins(0, 0, 0, 0);
    m_rumorKindCombo = new QComboBox(rumorPicker);
    m_rumorKindCombo->addItems({QStringLiteral("rumor"), QStringLiteral("rumor_type2")});
    auto *addRumor = new QPushButton(tr("Find Rumor..."), rumorPicker);
    auto *newRumor = new QPushButton(tr("New"), rumorPicker);
    auto *removeRumor = new QPushButton(tr("Remove Rumor"), rumorPicker);
    rumorPickerLayout->addWidget(new QLabel(tr("Type:"), rumorPicker));
    rumorPickerLayout->addWidget(m_rumorKindCombo);
    rumorPickerLayout->addStretch(1);
    rumorPickerLayout->addWidget(addRumor);
    rumorPickerLayout->addWidget(newRumor);
    rumorPickerLayout->addWidget(removeRumor);
    rumorLayout->addWidget(rumorPicker);
    m_rumorTable = new QTableWidget(rumorTab);
    m_rumorTable->setColumnCount(5);
    m_rumorTable->setHorizontalHeaderLabels({tr("Type"), tr("From"), tr("To"), tr("Weight"), tr("IDS")});
    m_rumorTable->horizontalHeader()->setStretchLastSection(true);
    m_rumorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    rumorLayout->addWidget(m_rumorTable, 1);
    m_rumorTextPreview = new QPlainTextEdit(rumorTab);
    m_rumorTextPreview->setReadOnly(true);
    m_rumorTextPreview->setMinimumHeight(120);
    m_rumorTextPreview->setPlaceholderText(tr("Select a rumor to see the text."));
    rumorLayout->addWidget(m_rumorTextPreview, 0);
    connect(addRumor, &QPushButton::clicked, this, &NpcEditorPage::onAddRumor);
    connect(newRumor, &QPushButton::clicked, this, &NpcEditorPage::onNewRumor);
    connect(removeRumor, &QPushButton::clicked, this, &NpcEditorPage::onRemoveRumor);
    connect(m_rumorTable, &QTableWidget::currentCellChanged, this, [this]() { refreshSelectedRumorText(); });
    tabs->addTab(rumorTab, tr("Rumors"));

    splitter->addWidget(left);
    splitter->addWidget(middle);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);

    auto *bottomBar = new QWidget(this);
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(8, 6, 8, 6);
    m_statusLabel = new QLabel(bottomBar);
    bottomLayout->addWidget(m_statusLabel, 1);
    m_bottomSaveButton = new QPushButton(tr("Save"), bottomBar);
    m_bottomSaveButton->setMinimumWidth(120);
    m_bottomSaveButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f8f4d; color: white; border: 1px solid #2fb365; padding: 6px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #24a95a; }"
        "QPushButton:pressed { background: #176d3a; }"
        "QPushButton:disabled { background: #405146; color: #a8b3ad; border-color: #4e6256; }"));
    bottomLayout->addWidget(m_bottomSaveButton);
    root->addWidget(bottomBar);
    connect(m_bottomSaveButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (!saveCurrentFile(&error)) {
            QMessageBox::warning(this, tr("NPC Editor"), error.isEmpty() ? tr("The NPC data could not be saved.") : error);
            return;
        }
        m_statusLabel->setText(tr("Saved: %1").arg(m_mbasesPath));
    });

    connect(m_individualNameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_individualNameTextEdit->hasFocus())
            m_individualNameTextEdit->setText(resolvedIdsText(value));
    });
    connect(m_infoSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_infoPreviewLabel->setText(resolvedIdsText(value));
    });

    setEditorEnabled(false);
}

void NpcEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);
    m_reloadAction = m_toolBar->addAction(tr("Reload"), this, &NpcEditorPage::scheduleReloadCurrentContext);
    m_toolBar->addSeparator();
    m_newNpcAction = m_toolBar->addAction(tr("New NPC"), this, &NpcEditorPage::onNewNpc);
    m_deleteNpcAction = m_toolBar->addAction(tr("Delete NPC"), this, &NpcEditorPage::onDeleteNpc);
    m_toolBar->addSeparator();
    m_toolBar->addWidget(new QLabel(tr(" System: "), m_toolBar));
    m_systemCombo = new QComboBox(m_toolBar);
    m_systemCombo->setMinimumWidth(180);
    m_toolBar->addWidget(m_systemCombo);
    m_toolBar->addWidget(new QLabel(tr(" Base: "), m_toolBar));
    m_baseCombo = new QComboBox(m_toolBar);
    m_baseCombo->setMinimumWidth(260);
    m_toolBar->addWidget(m_baseCombo);
    connect(m_systemCombo, &QComboBox::currentIndexChanged, this, &NpcEditorPage::onSystemChanged);
    connect(m_baseCombo, &QComboBox::currentIndexChanged, this, &NpcEditorPage::onBaseChanged);
}

void NpcEditorPage::scheduleReloadCurrentContext()
{
    if (m_reloadQueued)
        return;
    m_reloadQueued = true;
    if (m_statusLabel)
        m_statusLabel->setText(tr("Loading NPC data..."));
    QTimer::singleShot(0, this, [this]() {
        m_reloadQueued = false;
        reloadCurrentContext();
    });
}

void NpcEditorPage::reloadCurrentContext()
{
    if (m_loading) {
        scheduleReloadCurrentContext();
        return;
    }

    m_loading = true;
    if (m_reloadAction)
        m_reloadAction->setEnabled(false);
    if (m_newNpcAction)
        m_newNpcAction->setEnabled(false);
    if (m_deleteNpcAction)
        m_deleteNpcAction->setEnabled(false);
    if (m_bottomSaveButton)
        m_bottomSaveButton->setEnabled(false);

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    QString error;
    reportLoadingProgress(0, tr("NPC Editor: Preparing data..."));
    if (!loadGameRoot(gameRoot, &error)) {
        m_statusLabel->setText(error.isEmpty() ? tr("No active mod context.") : error);
        emit titleChanged(tr("NPC Editor"));
        reportLoadingProgress(100, error.isEmpty() ? tr("NPC Editor: no active mod context") : error);
        m_loading = false;
        if (m_reloadAction)
            m_reloadAction->setEnabled(true);
        if (m_newNpcAction)
            m_newNpcAction->setEnabled(false);
        if (m_deleteNpcAction)
            m_deleteNpcAction->setEnabled(false);
        if (m_bottomSaveButton)
            m_bottomSaveButton->setEnabled(false);
        return;
    }
    emit titleChanged(tr("NPC Editor"));
    reportLoadingProgress(100, tr("NPC Editor: %1 NPCs loaded").arg(npcCount()));
    m_loading = false;
    if (m_reloadAction)
        m_reloadAction->setEnabled(true);
    if (m_newNpcAction)
        m_newNpcAction->setEnabled(!m_bases.isEmpty());
    setEditorEnabled(currentNpc() != nullptr);
}

void NpcEditorPage::reportLoadingProgress(int percent, const QString &message)
{
    emit loadingProgressChanged(std::clamp(percent, 0, 100), message);
    if (m_statusLabel)
        m_statusLabel->setText(message);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

bool NpcEditorPage::loadGameRoot(const QString &gameRoot, QString *errorMessage)
{
    reportLoadingProgress(2, tr("NPC Editor: Checking context..."));
    m_gameRoot = gameRoot.trimmed();
    m_mbasesDoc.clear();
    m_systems.clear();
    m_bases.clear();
    m_existingRumors.clear();
    m_idsTextByNumber.clear();
    m_bodyChoices.clear();
    m_headChoices.clear();
    m_handChoices.clear();
    m_voiceChoices.clear();
    m_factionChoices.clear();
    m_factionDisplayByNickname.clear();
    m_dirtyBaseKeys.clear();
    clearEditor();

    if (m_gameRoot.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Please first select an installation for editing in Mod Manager.");
        populateSelectors();
        return false;
    }

    reportLoadingProgress(8, tr("NPC Editor: Searching Freelancer paths..."));
    const QString dataDir = dataDirForGameRoot(m_gameRoot);
    const QString universePath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    m_mbasesPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS/mbases.ini"));
    if (universePath.isEmpty() || m_mbasesPath.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("universe.ini or mbases.ini was not found in the active mod.");
        populateSelectors();
        return false;
    }

    reportLoadingProgress(18, tr("NPC Editor: Loading IDS texts..."));
    const IdsDataset idsDataset = IdsDataService::loadFromGameRoot(m_gameRoot);
    for (const auto &entry : idsDataset.entries) {
        QString value = entry.hasStringValue ? entry.stringValue : entry.plainText;
        if (!value.trimmed().isEmpty()) {
            m_idsTextByNumber.insert(QString::number(entry.globalId), value.trimmed());
            if (entry.localId > 0 && !m_idsTextByNumber.contains(QString::number(entry.localId)))
                m_idsTextByNumber.insert(QString::number(entry.localId), value.trimmed());
        }
    }
    if (m_idsTextByNumber.isEmpty()) {
        IdsStringTable ids;
        ids.loadFromFreelancerDir(exeDirForGameRoot(m_gameRoot));
        for (auto it = ids.strings().constBegin(); it != ids.strings().constEnd(); ++it)
            m_idsTextByNumber.insert(QString::number(it.key()), it.value());
    }

    reportLoadingProgress(30, tr("NPC Editor: Reading resource DLL..."));
    // Bribe text IDs in mbases.ini are local string-table IDs (commonly
    // 16100/16101) from resources.dll, not global Freelancer IDS values.
    // Some mods do not list resources.dll in [Resources], so load it directly.
    const QString resourcesDllPath = flatlas::core::PathUtils::ciResolvePath(exeDirForGameRoot(m_gameRoot),
                                                                             QStringLiteral("resources.dll"));
    if (!resourcesDllPath.isEmpty()) {
        const auto localStrings = DllResources::loadStrings(resourcesDllPath, 0, 65535);
        for (auto it = localStrings.constBegin(); it != localStrings.constEnd(); ++it) {
            const QString key = QString::number(it.key());
            if (!m_idsTextByNumber.contains(key))
                m_idsTextByNumber.insert(key, it.value().trimmed());
        }
    }

    reportLoadingProgress(38, tr("NPC Editor: Loading factions..."));
    const QString initialWorldPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("initialworld.ini"));
    const IniDocument initialWorldDoc = IniParser::parseFile(initialWorldPath);
    for (const IniSection &section : initialWorldDoc) {
        if (section.name.compare(QStringLiteral("Group"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = sectionValue(section, QStringLiteral("nickname"));
        const QString display = resolvedIdsText(toInt(section.value(QStringLiteral("ids_name")))).trimmed();
        appendUnique(&m_factionChoices, nickname);
        if (!nickname.isEmpty() && !display.isEmpty())
            m_factionDisplayByNickname.insert(keyOf(nickname), display);
    }

    reportLoadingProgress(48, tr("NPC Editor: Loading universe and bases..."));
    const IniDocument universeDoc = IniParser::parseFile(universePath);
    QSet<QString> seenSystems;
    QHash<QString, int> baseIndexByNickname;
    for (const IniSection &section : universeDoc) {
        if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0) {
            const QString nickname = sectionValue(section, QStringLiteral("nickname"));
            if (nickname.isEmpty())
                continue;
            const int idsName = toInt(section.value(QStringLiteral("strid_name")));
            if (seenSystems.contains(keyOf(nickname))) {
                for (NpcSystemRecord &existing : m_systems) {
                    if (existing.nickname.compare(nickname, Qt::CaseInsensitive) == 0) {
                        existing.displayName = displayLabel(nickname, resolvedIdsText(idsName));
                        break;
                    }
                }
                continue;
            }
            NpcSystemRecord system;
            system.nickname = nickname;
            system.displayName = displayLabel(nickname, resolvedIdsText(idsName));
            m_systems.append(system);
            seenSystems.insert(keyOf(nickname));
        } else if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) == 0) {
            NpcBaseRecord base;
            base.nickname = sectionValue(section, QStringLiteral("nickname"));
            base.systemNickname = sectionValue(section, QStringLiteral("system"));
            base.fileRelativePath = sectionValue(section, QStringLiteral("file"));
            const int idsName = toInt(section.value(QStringLiteral("strid_name")));
            base.displayName = displayLabel(base.nickname, resolvedIdsText(idsName));
            if (!base.nickname.isEmpty()) {
                baseIndexByNickname.insert(keyOf(base.nickname), m_bases.size());
                m_bases.append(base);
                if (!base.systemNickname.isEmpty() && !seenSystems.contains(keyOf(base.systemNickname))) {
                    NpcSystemRecord system;
                    system.nickname = base.systemNickname;
                    system.displayName = base.systemNickname;
                    m_systems.append(system);
                    seenSystems.insert(keyOf(base.systemNickname));
                }
            }
        }
    }

    std::sort(m_systems.begin(), m_systems.end(), [](const NpcSystemRecord &a, const NpcSystemRecord &b) {
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });
    std::sort(m_bases.begin(), m_bases.end(), [](const NpcBaseRecord &a, const NpcBaseRecord &b) {
        if (a.systemNickname.compare(b.systemNickname, Qt::CaseInsensitive) == 0)
            return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
        return a.systemNickname.compare(b.systemNickname, Qt::CaseInsensitive) < 0;
    });
    baseIndexByNickname.clear();
    for (int i = 0; i < m_bases.size(); ++i)
        baseIndexByNickname.insert(keyOf(m_bases.at(i).nickname), i);

    reportLoadingProgress(62, tr("NPC Editor: Reading base rooms..."));
    for (NpcBaseRecord &base : m_bases) {
        const QString absoluteBaseFile = flatlas::core::PathUtils::ciResolvePath(dataDir, base.fileRelativePath);
        const IniDocument baseDoc = absoluteBaseFile.isEmpty() ? IniDocument{} : IniParser::parseFile(absoluteBaseFile);
        for (const IniSection &section : baseDoc) {
            if (section.name.compare(QStringLiteral("Room"), Qt::CaseInsensitive) != 0)
                continue;
            NpcRoomRecord room;
            room.nickname = sectionValue(section, QStringLiteral("nickname"));
            if (!room.nickname.isEmpty())
                base.rooms.append(room);
        }
    }

    reportLoadingProgress(74, tr("NPC Editor: Evaluating mbases.ini..."));
    m_mbasesDoc = IniParser::parseFile(m_mbasesPath);
    QHash<int, int> bribePriceCounts;
    NpcBaseRecord *currentBase = nullptr;
    QHash<QString, QString> baseFactionByNpc;
    for (int i = 0; i < m_mbasesDoc.size(); ++i) {
        const IniSection &section = m_mbasesDoc.at(i);
        if (section.name.compare(QStringLiteral("MBase"), Qt::CaseInsensitive) == 0) {
            const QString baseNickname = sectionValue(section, QStringLiteral("nickname"));
            currentBase = baseIndexByNickname.contains(keyOf(baseNickname)) ? &m_bases[baseIndexByNickname.value(keyOf(baseNickname))] : nullptr;
            baseFactionByNpc.clear();
        } else if (currentBase && section.name.compare(QStringLiteral("BaseFaction"), Qt::CaseInsensitive) == 0) {
            const QString faction = sectionValue(section, QStringLiteral("faction"));
            appendUnique(&m_factionChoices, faction);
            for (const QString &npcName : section.values(QStringLiteral("npc")))
                baseFactionByNpc.insert(keyOf(npcName), faction);
        } else if (currentBase && section.name.compare(QStringLiteral("MRoom"), Qt::CaseInsensitive) == 0) {
            const QString roomName = sectionValue(section, QStringLiteral("nickname"));
            if (roomName.isEmpty())
                continue;
            auto roomIt = std::find_if(currentBase->rooms.begin(), currentBase->rooms.end(), [&](const NpcRoomRecord &room) {
                return room.nickname.compare(roomName, Qt::CaseInsensitive) == 0;
            });
            if (roomIt == currentBase->rooms.end()) {
                NpcRoomRecord room;
                room.nickname = roomName;
                currentBase->rooms.append(room);
                roomIt = currentBase->rooms.end() - 1;
            } else {
                roomIt->nickname = roomName;
            }
            for (const QString &fixture : section.values(QStringLiteral("fixture"))) {
                const QStringList parts = csvParts(fixture);
                if (!parts.isEmpty())
                    appendUnique(&roomIt->fixtureNpcs, parts.at(0));
            }
        } else if (currentBase && section.name.compare(QStringLiteral("GF_NPC"), Qt::CaseInsensitive) == 0) {
            NpcRecord npc;
            npc.sectionIndex = i;
            npc.baseNickname = currentBase->nickname;
            npc.nickname = sectionValue(section, QStringLiteral("nickname"));
            npc.room = sectionValue(section, QStringLiteral("room"));
            npc.baseFaction = baseFactionByNpc.value(keyOf(npc.nickname));
            for (const auto &entry : section.entries) {
                const QString k = keyOf(entry.first);
                if (k == QStringLiteral("body")) npc.body = entry.second.trimmed();
                else if (k == QStringLiteral("head")) npc.head = entry.second.trimmed();
                else if (k == QStringLiteral("lefthand")) npc.leftHand = entry.second.trimmed();
                else if (k == QStringLiteral("righthand")) npc.rightHand = entry.second.trimmed();
                else if (k == QStringLiteral("individual_name")) npc.individualName = toInt(entry.second);
                else if (k == QStringLiteral("info")) npc.info = toInt(entry.second);
                else if (k == QStringLiteral("affiliation")) npc.affiliation = entry.second.trimmed();
                else if (k == QStringLiteral("voice")) npc.voice = entry.second.trimmed();
                else if (k == QStringLiteral("npc_type")) npc.npcType = entry.second.trimmed();
                else if (k == QStringLiteral("bribe")) {
                    const NpcBribe bribe = parseBribe(entry.second);
                    npc.bribes.append(bribe);
                    if (bribe.price > 0)
                        bribePriceCounts[bribe.price] += 1;
                }
                else if (k == QStringLiteral("rumor") || k == QStringLiteral("rumor_type2")) {
                    const NpcRumorAssignment rumor = parseRumor(k, entry.second);
                    npc.rumors.append(rumor);
                    NpcExistingRumor existing;
                    existing.kind = rumor.kind;
                    existing.stateFrom = rumor.stateFrom;
                    existing.stateTo = rumor.stateTo;
                    existing.weight = rumor.weight;
                    existing.ids = rumor.ids;
                    existing.preview = resolvedIdsText(rumor.ids);
                    const bool duplicate = std::any_of(m_existingRumors.begin(), m_existingRumors.end(), [&](const NpcExistingRumor &other) {
                        return other.kind == existing.kind && other.stateFrom == existing.stateFrom && other.stateTo == existing.stateTo
                               && other.weight == existing.weight && other.ids == existing.ids;
                    });
                    if (!duplicate)
                        m_existingRumors.append(existing);
                } else if (!isKnownNpcKey(entry.first)) {
                    npc.preservedEntries.append(entry);
                }
            }
            appendUnique(&m_factionChoices, npc.baseFaction);
            appendUnique(&m_factionChoices, npc.affiliation);
            appendUnique(&m_bodyChoices, npc.body);
            appendUnique(&m_headChoices, npc.head);
            appendUnique(&m_handChoices, npc.leftHand);
            appendUnique(&m_handChoices, npc.rightHand);
            appendUnique(&m_voiceChoices, npc.voice);
            npc.individualNameText = resolvedIdsText(npc.individualName);
            if (npc.room.isEmpty()) {
                for (const NpcRoomRecord &room : std::as_const(currentBase->rooms)) {
                    if (room.fixtureNpcs.contains(npc.nickname, Qt::CaseInsensitive)) {
                        npc.room = room.nickname;
                        break;
                    }
                }
            }
            currentBase->npcs.append(npc);
        }
    }

    if (!bribePriceCounts.isEmpty()) {
        int bestPrice = 10000;
        int bestCount = -1;
        for (auto it = bribePriceCounts.constBegin(); it != bribePriceCounts.constEnd(); ++it) {
            if (it.value() > bestCount) {
                bestPrice = it.key();
                bestCount = it.value();
            }
        }
        m_modBribePrice = bestPrice;
    }

    reportLoadingProgress(90, tr("NPC Editor: Loading outfit and voice lists..."));
    const QString bodyparts = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("CHARACTERS/bodyparts.ini"));
    for (const IniSection &section : IniParser::parseFile(bodyparts)) {
        if (section.name.compare(QStringLiteral("Body"), Qt::CaseInsensitive) == 0)
            appendUnique(&m_bodyChoices, section.value(QStringLiteral("nickname")));
        else if (section.name.compare(QStringLiteral("Head"), Qt::CaseInsensitive) == 0)
            appendUnique(&m_headChoices, section.value(QStringLiteral("nickname")));
        else if (section.name.compare(QStringLiteral("Hand"), Qt::CaseInsensitive) == 0)
            appendUnique(&m_handChoices, section.value(QStringLiteral("nickname")));
    }

    const QString audioDirPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("AUDIO"));
    if (!audioDirPath.isEmpty()) {
        const QDir audioDir(audioDirPath);
        const QStringList audioFiles = audioDir.entryList({QStringLiteral("*.ini")}, QDir::Files, QDir::Name);
        for (const QString &fileName : audioFiles) {
            const IniDocument audioDoc = IniParser::parseFile(audioDir.absoluteFilePath(fileName));
            for (const IniSection &section : audioDoc) {
                if (section.name.compare(QStringLiteral("Voice"), Qt::CaseInsensitive) == 0)
                    appendUnique(&m_voiceChoices, section.value(QStringLiteral("nickname")));
            }
        }
    }

    m_bodyChoices.sort(Qt::CaseInsensitive);
    m_headChoices.sort(Qt::CaseInsensitive);
    m_handChoices.sort(Qt::CaseInsensitive);
    m_voiceChoices.sort(Qt::CaseInsensitive);
    m_factionChoices.sort(Qt::CaseInsensitive);
    reportLoadingProgress(96, tr("NPC Editor: Filling interface..."));
    populateChoiceLists();
    populateRumorChoices();
    populateSelectors();
    m_statusLabel->setText(tr("Loaded: %1 bases, %2 rumor templates").arg(m_bases.size()).arg(m_existingRumors.size()));
    return true;
}

bool NpcEditorPage::saveCurrentFile(QString *errorMessage)
{
    saveEditorToCurrentNpc();
    if (!applyIdsNameEdits(errorMessage))
        return false;
    for (const NpcBaseRecord &base : std::as_const(m_bases)) {
        QSet<QString> seen;
        for (const NpcRecord &npc : base.npcs) {
            if (npc.nickname.trimmed().isEmpty()) {
                if (errorMessage)
                    *errorMessage = tr("Base %1 contains an NPC without a nickname.").arg(base.nickname);
                return false;
            }
            const QString npcKey = keyOf(npc.nickname);
            if (seen.contains(npcKey)) {
                if (errorMessage)
                    *errorMessage = tr("Base %1 contains duplicate NPC nickname %2.").arg(base.nickname, npc.nickname);
                return false;
            }
            seen.insert(npcKey);
        }
    }
    if (m_mbasesPath.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("No mbases.ini loaded.");
        return false;
    }

    QSet<QString> dirtyBaseKeys = m_dirtyBaseKeys;
    for (const NpcBaseRecord &base : std::as_const(m_bases)) {
        for (const NpcRecord &npc : base.npcs) {
            if (npc.newlyCreated || npc.dirty) {
                dirtyBaseKeys.insert(keyOf(base.nickname));
                break;
            }
        }
    }

    if (dirtyBaseKeys.isEmpty())
        return true;

    IniDocument newDoc = m_mbasesDoc;
    QSet<QString> processedBases;
    for (const NpcBaseRecord &base : std::as_const(m_bases)) {
        const QString baseKey = keyOf(base.nickname);
        if (!dirtyBaseKeys.contains(baseKey))
            continue;
        if (processedBases.contains(baseKey))
            continue;
        processedBases.insert(baseKey);

        const QVector<int> oldIndexes = sectionIndexesForMbase(newDoc, base.nickname);
        IniDocument block;
        if (!oldIndexes.isEmpty()) {
            block = rebuildMbaseBlockPreservingOrder(newDoc, oldIndexes, base, m_modBribePrice);
        } else if (!base.npcs.isEmpty()) {
            const QHash<QString, QStringList> npcsByFaction = npcNamesByFaction(base);
            IniSection mbase;
            mbase.name = QStringLiteral("MBase");
            mbase.entries.append({QStringLiteral("nickname"), base.nickname});
            block.append(mbase);
            for (auto it = npcsByFaction.constBegin(); it != npcsByFaction.constEnd(); ++it) {
                IniSection faction;
                faction.name = QStringLiteral("BaseFaction");
                faction.entries.append({QStringLiteral("faction"), it.key()});
                for (const QString &npcName : it.value())
                    faction.entries.append({QStringLiteral("npc"), npcName});
                block.append(faction);
            }
            for (const NpcRecord &npc : base.npcs)
                block.append(npcToSection(npc, m_modBribePrice));
        }

        if (!oldIndexes.isEmpty()) {
            const int first = oldIndexes.first();
            const int count = oldIndexes.size();
            for (int i = 0; i < count; ++i)
                newDoc.removeAt(first);
            for (int i = 0; i < block.size(); ++i)
                newDoc.insert(first + i, block.at(i));
        } else if (!block.isEmpty()) {
            for (const IniSection &section : std::as_const(block))
                newDoc.append(section);
        }
    }

    QFile file(m_mbasesPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = tr("mbases.ini could not be written: %1").arg(m_mbasesPath);
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << IniParser::serialize(newDoc);
    m_mbasesDoc = newDoc;
    m_dirtyBaseKeys.clear();
    for (NpcBaseRecord &base : m_bases) {
        for (NpcRecord &npc : base.npcs) {
            npc.dirty = false;
            npc.newlyCreated = false;
        }
    }
    return true;
}

void NpcEditorPage::populateSelectors()
{
    m_populating = true;
    m_systemCombo->clear();
    for (const NpcSystemRecord &system : std::as_const(m_systems))
        m_systemCombo->addItem(system.displayName, system.nickname);
    m_populating = false;
    populateBaseSelector();
}

void NpcEditorPage::populateBaseSelector()
{
    m_populating = true;
    const QString system = m_systemCombo->currentData().toString();
    m_baseCombo->clear();
    for (const NpcBaseRecord &base : std::as_const(m_bases)) {
        if (base.systemNickname.compare(system, Qt::CaseInsensitive) == 0)
            m_baseCombo->addItem(base.displayName, base.nickname);
    }
    m_populating = false;
    populateRooms();
}

void NpcEditorPage::populateRooms()
{
    m_populating = true;
    m_roomList->clear();
    const NpcBaseRecord *base = currentBase();
    if (base) {
        m_roomCombo->clear();
        m_roomCombo->addItems(allRoomNames(*base));
        for (const QString &room : allRoomNames(*base))
            m_roomList->addItem(room);
        if (m_roomList->count() > 0)
            m_roomList->setCurrentRow(0);
    }
    m_populating = false;
    populateNpcTable();
}

void NpcEditorPage::populateNpcTable()
{
    m_currentNpcRow = -1;
    m_npcTable->setRowCount(0);
    const NpcBaseRecord *base = currentBase();
    if (!base) {
        clearEditor();
        return;
    }
    const QString selectedRoom = m_roomList->currentItem() ? m_roomList->currentItem()->text() : QString();
    for (int i = 0; i < base->npcs.size(); ++i) {
        const NpcRecord &npc = base->npcs.at(i);
        const QString effectiveRoom = npc.room.trimmed().isEmpty() ? tr("Not assigned") : npc.room;
        if (effectiveRoom.compare(selectedRoom, Qt::CaseInsensitive) != 0)
            continue;
        const int row = m_npcTable->rowCount();
        m_npcTable->insertRow(row);
        m_npcTable->setItem(row, 0, readOnlyItem(npc.nickname));
        m_npcTable->setItem(row, 1, readOnlyItem(resolvedIdsText(npc.individualName)));
        m_npcTable->setItem(row, 2, readOnlyItem(factionDisplay(npc.baseFaction)));
        m_npcTable->setItem(row, 3, readOnlyItem(factionDisplay(npc.affiliation)));
        m_npcTable->setItem(row, 4, readOnlyItem(QString::number(npc.bribes.size())));
        m_npcTable->setItem(row, 5, readOnlyItem(QString::number(npc.rumors.size())));
        m_npcTable->item(row, 0)->setData(Qt::UserRole, i);
    }
    if (m_npcTable->rowCount() > 0)
        m_npcTable->setCurrentCell(0, 0);
    else
        clearEditor();
}

void NpcEditorPage::populateEditor()
{
    const NpcRecord *npc = currentNpc();
    if (!npc) {
        clearEditor();
        return;
    }
    const NpcBaseRecord *base = currentBase();
    m_editorBaseNickname = base ? base->nickname : QString();
    m_editorNpcIndex = currentNpcIndex();
    setEditorEnabled(true);
    m_nicknameEdit->setText(npc->nickname);
    setComboText(m_roomCombo, npc->room);
    setFactionComboNickname(m_baseFactionCombo, npc->baseFaction);
    setFactionComboNickname(m_affiliationCombo, npc->affiliation);
    setComboText(m_bodyCombo, npc->body);
    setComboText(m_headCombo, npc->head);
    setComboText(m_leftHandCombo, npc->leftHand);
    setComboText(m_rightHandCombo, npc->rightHand);
    setComboText(m_voiceCombo, npc->voice);
    m_individualNameSpin->setValue(npc->individualName);
    m_individualNameTextEdit->setText(npc->individualNameText.trimmed().isEmpty()
                                          ? resolvedIdsText(npc->individualName)
                                          : npc->individualNameText);
    m_infoSpin->setValue(npc->info);

    m_bribeTable->setRowCount(npc->bribes.size());
    for (int row = 0; row < npc->bribes.size(); ++row) {
        const NpcBribe &bribe = npc->bribes.at(row);
        m_bribeTable->setItem(row, 0, readOnlyItem(m_factionDisplayByNickname.value(keyOf(bribe.faction), bribe.faction)));
        m_bribeTable->setItem(row, 1, readOnlyItem(bribe.faction));
        m_bribeTable->item(row, 0)->setData(Qt::UserRole, bribe.ids);
    }
    if (m_bribeTable->rowCount() > 0)
        m_bribeTable->setCurrentCell(0, 0);
    else
        refreshSelectedBribeText();

    m_rumorTable->setRowCount(npc->rumors.size());
    for (int row = 0; row < npc->rumors.size(); ++row) {
        const NpcRumorAssignment &rumor = npc->rumors.at(row);
        m_rumorTable->setItem(row, 0, new QTableWidgetItem(rumor.kind));
        m_rumorTable->setItem(row, 1, new QTableWidgetItem(rumor.stateFrom));
        m_rumorTable->setItem(row, 2, new QTableWidgetItem(rumor.stateTo));
        m_rumorTable->setItem(row, 3, new QTableWidgetItem(QString::number(rumor.weight)));
        m_rumorTable->setItem(row, 4, new QTableWidgetItem(QString::number(rumor.ids)));
        m_rumorTable->item(row, 4)->setData(Qt::UserRole, rumor.ids);
    }
    m_rumorTable->resizeColumnsToContents();
    if (m_rumorTable->rowCount() > 0)
        m_rumorTable->setCurrentCell(0, 0);
    else
        refreshSelectedRumorText();
}

void NpcEditorPage::populateChoiceLists()
{
    const QStringList emptyPlusBodies = QStringList{QString()} + m_bodyChoices;
    const QStringList emptyPlusHeads = QStringList{QString()} + m_headChoices;
    const QStringList emptyPlusHands = QStringList{QString()} + m_handChoices;
    const QStringList emptyPlusVoices = QStringList{QString()} + m_voiceChoices;
    m_bodyCombo->clear(); m_bodyCombo->addItems(emptyPlusBodies);
    m_headCombo->clear(); m_headCombo->addItems(emptyPlusHeads);
    m_leftHandCombo->clear(); m_leftHandCombo->addItems(emptyPlusHands);
    m_rightHandCombo->clear(); m_rightHandCombo->addItems(emptyPlusHands);
    m_voiceCombo->clear(); m_voiceCombo->addItems(emptyPlusVoices);
    for (QComboBox *combo : {m_baseFactionCombo, m_affiliationCombo}) {
        combo->clear();
        combo->addItem(QString(), QString());
        for (const QString &nickname : std::as_const(m_factionChoices))
            combo->addItem(factionDisplay(nickname), nickname);
    }
}

bool NpcEditorPage::applyIdsNameEdits(QString *errorMessage)
{
    bool needsDataset = false;
    for (const NpcBaseRecord &base : std::as_const(m_bases)) {
        for (const NpcRecord &npc : base.npcs) {
            const QString desiredName = npc.individualNameText.trimmed();
            const QString currentName = resolvedIdsText(npc.individualName).trimmed();
            if (desiredName.isEmpty())
                continue;
            if (npc.individualName <= 0 || desiredName.compare(currentName, Qt::CaseSensitive) != 0) {
                needsDataset = true;
                break;
            }
        }
        if (needsDataset)
            break;
    }

    IdsDataset dataset;
    QString targetDll;
    if (needsDataset) {
        dataset = IdsDataService::loadFromGameRoot(m_gameRoot);
        targetDll = IdsDataService::defaultCreationDllName(dataset);
        if (targetDll.trimmed().isEmpty()) {
            if (errorMessage)
                *errorMessage = tr("No target DLL could be determined for the NPC name.");
            return false;
        }
    }

    for (NpcBaseRecord &base : m_bases) {
        for (NpcRecord &npc : base.npcs) {
            const QString desiredName = npc.individualNameText.trimmed();
            if (desiredName.isEmpty()) {
                npc.individualName = 0;
                continue;
            }

            const QString currentName = resolvedIdsText(npc.individualName).trimmed();
            if (npc.individualName > 0 && desiredName.compare(currentName, Qt::CaseSensitive) == 0)
                continue;

            int newGlobalId = npc.individualName;
            QString idsError;
            if (!IdsDataService::writeStringEntry(dataset,
                                                  targetDll,
                                                  npc.individualName,
                                                  desiredName,
                                                  &newGlobalId,
                                                  &idsError)) {
                if (errorMessage)
                    *errorMessage = tr("NPC name for %1 could not be saved: %2")
                                        .arg(npc.nickname, idsError);
                return false;
            }
            npc.individualName = newGlobalId;
            npc.dirty = true;
            m_dirtyBaseKeys.insert(keyOf(base.nickname));
            m_idsTextByNumber.insert(QString::number(newGlobalId), desiredName);
        }
    }
    return true;
}

void NpcEditorPage::populateRumorChoices()
{
    // Rumors can be thousands of rows. Selection is handled by a searchable
    // dialog in onAddRumor instead of a very large combo box.
}

void NpcEditorPage::clearEditor()
{
    m_editorBaseNickname.clear();
    m_editorNpcIndex = -1;
    setEditorEnabled(false);
    m_nicknameEdit->clear();
    for (QComboBox *combo : {m_roomCombo, m_baseFactionCombo, m_affiliationCombo, m_bodyCombo, m_headCombo, m_leftHandCombo, m_rightHandCombo, m_voiceCombo})
        combo->setCurrentText(QString());
    m_individualNameSpin->setValue(0);
    m_individualNameTextEdit->clear();
    m_infoSpin->setValue(0);
    m_namePreviewLabel->clear();
    m_infoPreviewLabel->clear();
    m_bribeTable->setRowCount(0);
    if (m_bribeTextPreview)
        m_bribeTextPreview->clear();
    m_rumorTable->setRowCount(0);
    if (m_rumorTextPreview)
        m_rumorTextPreview->clear();
}

void NpcEditorPage::setEditorEnabled(bool enabled)
{
    const QVector<QWidget *> widgets = {
        m_nicknameEdit,      m_roomCombo,    m_baseFactionCombo, m_affiliationCombo,
        m_bodyCombo,         m_headCombo,    m_leftHandCombo,    m_rightHandCombo,
        m_voiceCombo,        m_individualNameSpin, m_individualNameTextEdit, m_infoSpin,
        m_bribeTable,       m_rumorTable,   m_rumorKindCombo
    };
    for (QWidget *widget : widgets)
        widget->setEnabled(enabled);
    if (m_deleteNpcAction)
        m_deleteNpcAction->setEnabled(enabled);
    if (m_bottomSaveButton)
        m_bottomSaveButton->setEnabled(!m_mbasesPath.isEmpty());
}

void NpcEditorPage::saveEditorToCurrentNpc()
{
    if (m_populating)
        return;
    NpcRecord *npc = nullptr;
    for (NpcBaseRecord &base : m_bases) {
        if (base.nickname.compare(m_editorBaseNickname, Qt::CaseInsensitive) != 0)
            continue;
        if (m_editorNpcIndex >= 0 && m_editorNpcIndex < base.npcs.size())
            npc = &base.npcs[m_editorNpcIndex];
        break;
    }
    if (!npc)
        return;
    NpcRecord updated = *npc;
    updated.nickname = m_nicknameEdit->text().trimmed();
    updated.room = m_roomCombo->currentText().trimmed();
    if (updated.room == tr("Not assigned"))
        updated.room.clear();
    for (const NpcBaseRecord &base : std::as_const(m_bases)) {
        if (base.nickname.compare(m_editorBaseNickname, Qt::CaseInsensitive) == 0) {
            updated.room = canonicalRoomName(base, updated.room);
            break;
        }
    }
    updated.baseFaction = currentFactionComboNickname(m_baseFactionCombo);
    updated.affiliation = currentFactionComboNickname(m_affiliationCombo);
    updated.body = m_bodyCombo->currentText().trimmed();
    updated.head = m_headCombo->currentText().trimmed();
    updated.leftHand = m_leftHandCombo->currentText().trimmed();
    updated.rightHand = m_rightHandCombo->currentText().trimmed();
    updated.voice = m_voiceCombo->currentText().trimmed();
    updated.individualName = m_individualNameSpin->value();
    updated.individualNameText = m_individualNameTextEdit->text().trimmed();
    updated.info = m_infoSpin->value();

    updated.bribes.clear();
    for (int row = 0; row < m_bribeTable->rowCount(); ++row) {
        NpcBribe bribe;
        bribe.faction = m_bribeTable->item(row, 1) ? m_bribeTable->item(row, 1)->text().trimmed() : QString();
        bribe.price = m_modBribePrice;
        bribe.ids = m_bribeTable->item(row, 0) ? m_bribeTable->item(row, 0)->data(Qt::UserRole).toInt() : 0;
        if (!bribe.faction.isEmpty())
            updated.bribes.append(bribe);
    }

    updated.rumors.clear();
    for (int row = 0; row < m_rumorTable->rowCount(); ++row) {
        NpcRumorAssignment rumor;
        rumor.kind = m_rumorTable->item(row, 0) ? m_rumorTable->item(row, 0)->text().trimmed() : QStringLiteral("rumor");
        rumor.stateFrom = m_rumorTable->item(row, 1) ? m_rumorTable->item(row, 1)->text().trimmed() : QString();
        rumor.stateTo = m_rumorTable->item(row, 2) ? m_rumorTable->item(row, 2)->text().trimmed() : QString();
        rumor.weight = m_rumorTable->item(row, 3) ? toInt(m_rumorTable->item(row, 3)->text(), 1) : 1;
        rumor.ids = m_rumorTable->item(row, 4) ? m_rumorTable->item(row, 4)->data(Qt::UserRole).toInt() : 0;
        if (rumor.ids <= 0 && m_rumorTable->item(row, 4))
            rumor.ids = toInt(m_rumorTable->item(row, 4)->text().section(QLatin1Char('-'), 0, 0));
        if (!rumor.kind.isEmpty() && rumor.ids > 0)
            updated.rumors.append(rumor);
    }

    if (!npcEditableFieldsEqual(*npc, updated)) {
        updated.dirty = true;
        m_dirtyBaseKeys.insert(keyOf(updated.baseNickname));
    }
    *npc = updated;
}

bool NpcEditorPage::validateEditor(QString *errorMessage) const
{
    const NpcRecord *npc = currentNpc();
    const NpcBaseRecord *base = currentBase();
    if (!npc || !base)
        return true;
    const QString nickname = m_nicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("The NPC nickname must not be empty.");
        return false;
    }
    int duplicates = 0;
    for (const NpcRecord &other : base->npcs) {
        if (other.nickname.compare(nickname, Qt::CaseInsensitive) == 0)
            ++duplicates;
    }
    if (duplicates > 1) {
        if (errorMessage)
            *errorMessage = tr("The NPC nickname must be unique within the base.");
        return false;
    }
    return true;
}

int NpcEditorPage::currentSystemIndex() const
{
    return m_systemCombo->currentIndex();
}

int NpcEditorPage::currentBaseIndex() const
{
    const QString baseNickname = m_baseCombo->currentData().toString();
    for (int i = 0; i < m_bases.size(); ++i) {
        if (m_bases.at(i).nickname.compare(baseNickname, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

int NpcEditorPage::currentRoomIndex() const
{
    return m_roomList->currentRow();
}

int NpcEditorPage::currentNpcIndex() const
{
    if (m_currentNpcRow < 0 || m_currentNpcRow >= m_npcTable->rowCount())
        return -1;
    const QTableWidgetItem *item = m_npcTable->item(m_currentNpcRow, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

NpcBaseRecord *NpcEditorPage::currentBase()
{
    const int index = currentBaseIndex();
    return index >= 0 && index < m_bases.size() ? &m_bases[index] : nullptr;
}

const NpcBaseRecord *NpcEditorPage::currentBase() const
{
    const int index = currentBaseIndex();
    return index >= 0 && index < m_bases.size() ? &m_bases[index] : nullptr;
}

NpcRecord *NpcEditorPage::currentNpc()
{
    NpcBaseRecord *base = currentBase();
    const int index = currentNpcIndex();
    return base && index >= 0 && index < base->npcs.size() ? &base->npcs[index] : nullptr;
}

const NpcRecord *NpcEditorPage::currentNpc() const
{
    const NpcBaseRecord *base = currentBase();
    const int index = currentNpcIndex();
    return base && index >= 0 && index < base->npcs.size() ? &base->npcs[index] : nullptr;
}

QString NpcEditorPage::resolvedIdsText(int ids) const
{
    if (ids <= 0)
        return {};
    return m_idsTextByNumber.value(QString::number(ids), tr("<IDS %1 not found>").arg(ids)).trimmed();
}

QString NpcEditorPage::displayLabel(const QString &nickname, const QString &resolved) const
{
    const QString cleanResolved = resolved.trimmed();
    return cleanResolved.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, cleanResolved);
}

QString NpcEditorPage::factionDisplay(const QString &nickname) const
{
    const QString cleanNickname = nickname.trimmed();
    if (cleanNickname.isEmpty())
        return {};
    const QString display = m_factionDisplayByNickname.value(keyOf(cleanNickname)).trimmed();
    return display.isEmpty() ? cleanNickname : QStringLiteral("%1 (%2)").arg(display, cleanNickname);
}

QString NpcEditorPage::factionNicknameFromDisplay(const QString &display) const
{
    const QString clean = display.trimmed();
    if (clean.isEmpty())
        return {};
    for (const QString &nickname : m_factionChoices) {
        if (clean.compare(nickname, Qt::CaseInsensitive) == 0
            || clean.compare(factionDisplay(nickname), Qt::CaseInsensitive) == 0
            || clean.compare(m_factionDisplayByNickname.value(keyOf(nickname)), Qt::CaseInsensitive) == 0) {
            return nickname;
        }
    }
    return clean;
}

QString NpcEditorPage::currentFactionComboNickname(const QComboBox *combo) const
{
    if (!combo)
        return {};
    const QString data = combo->currentData().toString().trimmed();
    if (!data.isEmpty())
        return data;
    return factionNicknameFromDisplay(combo->currentText());
}

void NpcEditorPage::setFactionComboNickname(QComboBox *combo, const QString &nickname)
{
    if (!combo)
        return;
    const int index = combo->findData(nickname);
    if (index >= 0)
        combo->setCurrentIndex(index);
    else
        combo->setCurrentText(factionDisplay(nickname));
}

QString NpcEditorPage::bribePreviewText(int row) const
{
    if (!m_bribeTable || row < 0 || row >= m_bribeTable->rowCount())
        return {};
    const QString factionNickname = m_bribeTable->item(row, 1) ? m_bribeTable->item(row, 1)->text().trimmed() : QString();
    int ids = m_bribeTable->item(row, 0) ? m_bribeTable->item(row, 0)->data(Qt::UserRole).toInt() : 0;
    if (ids <= 0)
        ids = 16100;
    QString text = resolvedIdsText(ids);
    if (text.isEmpty())
        return {};

    text.replace(QStringLiteral("%d0"), QString::number(m_modBribePrice));
    text.replace(QStringLiteral("%F0v1"), factionDisplay(factionNickname));
    text.replace(QStringLiteral("%F1v1"), tr("[affected opposing faction 1]"));
    text.replace(QStringLiteral("%F2v1"), tr("[affected opposing faction 2]"));
    text.replace(QStringLiteral("%F3v1"), tr("[affected opposing faction 3]"));
    return text;
}

QStringList NpcEditorPage::allRoomNames(const NpcBaseRecord &base) const
{
    QStringList rooms;
    for (const NpcRoomRecord &room : base.rooms)
        appendUnique(&rooms, room.nickname);
    for (const NpcRecord &npc : base.npcs)
        appendUnique(&rooms, npc.room);
    appendUnique(&rooms, tr("Not assigned"));
    return rooms;
}

QStringList NpcEditorPage::allFactions() const
{
    return m_factionChoices;
}

void NpcEditorPage::refreshSelectedBribeText()
{
    if (!m_bribeTextPreview)
        return;
    const int row = m_bribeTable ? m_bribeTable->currentRow() : -1;
    m_bribeTextPreview->setPlainText(bribePreviewText(row));
}

void NpcEditorPage::refreshSelectedRumorText()
{
    if (!m_rumorTextPreview)
        return;
    const int row = m_rumorTable ? m_rumorTable->currentRow() : -1;
    if (row < 0 || !m_rumorTable || row >= m_rumorTable->rowCount()) {
        m_rumorTextPreview->clear();
        return;
    }
    int ids = 0;
    if (auto *item = m_rumorTable->item(row, 4)) {
        ids = item->data(Qt::UserRole).toInt();
        if (ids <= 0)
            ids = toInt(item->text());
    }
    m_rumorTextPreview->setPlainText(resolvedIdsText(ids));
}

void NpcEditorPage::onSystemChanged()
{
    if (m_populating)
        return;
    populateBaseSelector();
}

void NpcEditorPage::onBaseChanged()
{
    if (m_populating)
        return;
    saveEditorToCurrentNpc();
    populateRooms();
}

void NpcEditorPage::onRoomChanged()
{
    if (m_populating)
        return;
    saveEditorToCurrentNpc();
    populateNpcTable();
}

void NpcEditorPage::onNpcSelectionChanged()
{
    if (m_populating)
        return;
    saveEditorToCurrentNpc();
    m_currentNpcRow = m_npcTable->currentRow();
    populateEditor();
}

void NpcEditorPage::onNewNpc()
{
    NpcBaseRecord *base = currentBase();
    if (!base)
        return;
    saveEditorToCurrentNpc();
    NpcRecord npc;
    npc.baseNickname = base->nickname;
    npc.room = m_roomList->currentItem() ? m_roomList->currentItem()->text() : QString();
    if (npc.room == tr("Not assigned"))
        npc.room.clear();
    npc.room = canonicalRoomName(*base, npc.room);
    npc.baseFaction = m_factionChoices.isEmpty() ? QString() : m_factionChoices.first();
    npc.affiliation = npc.baseFaction;
    npc.newlyCreated = true;
    npc.dirty = true;
    QString stem = base->nickname.toLower() + QStringLiteral("_npc");
    int counter = base->npcs.size() + 1;
    QSet<QString> existing;
    for (const NpcRecord &record : std::as_const(base->npcs))
        existing.insert(keyOf(record.nickname));
    do {
        npc.nickname = QStringLiteral("%1_%2").arg(stem).arg(counter++, 3, 10, QLatin1Char('0'));
    } while (existing.contains(keyOf(npc.nickname)));
    base->npcs.append(npc);
    m_dirtyBaseKeys.insert(keyOf(base->nickname));
    populateNpcTable();
    for (int row = 0; row < m_npcTable->rowCount(); ++row) {
        const QTableWidgetItem *item = m_npcTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == base->npcs.size() - 1) {
            m_npcTable->setCurrentCell(row, 0);
            break;
        }
    }
}

void NpcEditorPage::onDeleteNpc()
{
    NpcBaseRecord *base = currentBase();
    const int npcIndex = currentNpcIndex();
    if (!base || npcIndex < 0 || npcIndex >= base->npcs.size())
        return;
    const QString nickname = base->npcs.at(npcIndex).nickname;
    if (QMessageBox::question(this,
                              tr("Delete NPC"),
                              tr("Really remove NPC %1 from this base?").arg(nickname))
        != QMessageBox::Yes) {
        return;
    }
    base->npcs.removeAt(npcIndex);
    m_dirtyBaseKeys.insert(keyOf(base->nickname));
    populateNpcTable();
}

void NpcEditorPage::onAddBribe()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Select Bribe Faction"));
    dialog.resize(720, 520);
    auto *layout = new QVBoxLayout(&dialog);
    auto *searchEdit = new QLineEdit(&dialog);
    searchEdit->setPlaceholderText(tr("Search faction by ingame name or nickname..."));
    layout->addWidget(searchEdit);

    auto *choiceTable = new QTableWidget(&dialog);
    choiceTable->setColumnCount(2);
    choiceTable->setHorizontalHeaderLabels({tr("Faction"), tr("Nickname")});
    choiceTable->verticalHeader()->setVisible(false);
    choiceTable->setAlternatingRowColors(true);
    choiceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    choiceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    choiceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    choiceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    choiceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(choiceTable, 1);

    auto *idsBox = new QGroupBox(tr("Bribe Text"), &dialog);
    auto *idsLayout = new QFormLayout(idsBox);
    auto *idsCombo = new QComboBox(idsBox);
    idsCombo->addItem(QStringLiteral("16100 - %1").arg(resolvedIdsText(16100)), 16100);
    idsCombo->addItem(QStringLiteral("16101 - %1").arg(resolvedIdsText(16101)), 16101);
    idsLayout->addRow(tr("IDS:"), idsCombo);
    layout->addWidget(idsBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    auto refillChoices = [this, choiceTable](const QString &filterText) {
        choiceTable->setRowCount(0);
        const QStringList terms = filterText.trimmed().toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        for (const QString &nickname : m_factionChoices) {
            const QString displayName = m_factionDisplayByNickname.value(keyOf(nickname));
            const QString haystack = QStringLiteral("%1 %2 %3").arg(factionDisplay(nickname), displayName, nickname).toLower();
            bool matches = true;
            for (const QString &term : terms) {
                if (!haystack.contains(term)) {
                    matches = false;
                    break;
                }
            }
            if (!matches)
                continue;
            const int row = choiceTable->rowCount();
            choiceTable->insertRow(row);
            choiceTable->setItem(row, 0, readOnlyItem(factionDisplay(nickname)));
            choiceTable->setItem(row, 1, readOnlyItem(nickname));
            choiceTable->item(row, 0)->setData(Qt::UserRole, nickname);
        }
        if (choiceTable->rowCount() > 0)
            choiceTable->setCurrentCell(0, 0);
    };

    QObject::connect(searchEdit, &QLineEdit::textChanged, &dialog, refillChoices);
    QObject::connect(choiceTable, &QTableWidget::cellDoubleClicked, &dialog, [&dialog]() { dialog.accept(); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    refillChoices(QString());
    if (dialog.exec() != QDialog::Accepted)
        return;
    const int selectedRow = choiceTable->currentRow();
    if (selectedRow < 0 || !choiceTable->item(selectedRow, 0))
        return;
    const QString factionNickname = choiceTable->item(selectedRow, 0)->data(Qt::UserRole).toString().trimmed();
    if (factionNickname.isEmpty())
        return;

    const int row = m_bribeTable->rowCount();
    m_bribeTable->insertRow(row);
    m_bribeTable->setItem(row, 0, readOnlyItem(m_factionDisplayByNickname.value(keyOf(factionNickname), factionNickname)));
    m_bribeTable->setItem(row, 1, readOnlyItem(factionNickname));
    m_bribeTable->item(row, 0)->setData(Qt::UserRole, idsCombo->currentData().toInt());
    m_bribeTable->setCurrentCell(row, 0);
    refreshSelectedBribeText();
}

void NpcEditorPage::onRemoveBribe()
{
    const int row = m_bribeTable->currentRow();
    if (row >= 0) {
        m_bribeTable->removeRow(row);
        refreshSelectedBribeText();
    }
}

void NpcEditorPage::onAddRumor()
{
    if (m_existingRumors.isEmpty())
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Select Rumor"));
    dialog.resize(980, 620);
    auto *layout = new QVBoxLayout(&dialog);
    auto *searchEdit = new QLineEdit(&dialog);
    searchEdit->setPlaceholderText(tr("Search by text, IDS, type, or state..."));
    layout->addWidget(searchEdit);

    auto *choiceTable = new QTableWidget(&dialog);
    choiceTable->setColumnCount(6);
    choiceTable->setHorizontalHeaderLabels({tr("Type"), tr("From"), tr("To"), tr("Weight"), tr("IDS"), tr("Text")});
    choiceTable->verticalHeader()->setVisible(false);
    choiceTable->setAlternatingRowColors(true);
    choiceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    choiceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    choiceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    choiceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    choiceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    choiceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    choiceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    choiceTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    choiceTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    layout->addWidget(choiceTable, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    auto addChoiceRow = [this, choiceTable](int sourceIndex) {
        const NpcExistingRumor &rumor = m_existingRumors.at(sourceIndex);
        const int row = choiceTable->rowCount();
        choiceTable->insertRow(row);
        choiceTable->setItem(row, 0, readOnlyItem(rumor.kind));
        choiceTable->setItem(row, 1, readOnlyItem(rumor.stateFrom));
        choiceTable->setItem(row, 2, readOnlyItem(rumor.stateTo));
        choiceTable->setItem(row, 3, readOnlyItem(QString::number(rumor.weight)));
        choiceTable->setItem(row, 4, readOnlyItem(QString::number(rumor.ids)));
        choiceTable->setItem(row, 5, readOnlyItem(rumor.preview));
        choiceTable->item(row, 0)->setData(Qt::UserRole, sourceIndex);
    };

    auto refillChoices = [this, choiceTable, addChoiceRow](const QString &filterText) {
        choiceTable->setRowCount(0);
        const QStringList terms = filterText.trimmed().toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        int added = 0;
        constexpr int MaxVisibleRows = 500;
        for (int i = 0; i < m_existingRumors.size(); ++i) {
            const NpcExistingRumor &rumor = m_existingRumors.at(i);
            const QString haystack = QStringLiteral("%1 %2 %3 %4 %5 %6")
                                        .arg(rumor.kind,
                                             rumor.stateFrom,
                                             rumor.stateTo,
                                             QString::number(rumor.weight),
                                             QString::number(rumor.ids),
                                             rumor.preview)
                                        .toLower();
            bool matches = true;
            for (const QString &term : terms) {
                if (!haystack.contains(term)) {
                    matches = false;
                    break;
                }
            }
            if (!matches)
                continue;
            addChoiceRow(i);
            if (++added >= MaxVisibleRows)
                break;
        }
        if (choiceTable->rowCount() > 0)
            choiceTable->setCurrentCell(0, 0);
    };

    QObject::connect(searchEdit, &QLineEdit::textChanged, &dialog, refillChoices);
    QObject::connect(choiceTable, &QTableWidget::cellDoubleClicked, &dialog, [&dialog]() { dialog.accept(); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    refillChoices(QString());
    if (dialog.exec() != QDialog::Accepted)
        return;

    const int selectedRow = choiceTable->currentRow();
    if (selectedRow < 0 || !choiceTable->item(selectedRow, 0))
        return;
    const int choiceIndex = choiceTable->item(selectedRow, 0)->data(Qt::UserRole).toInt();
    if (choiceIndex < 0 || choiceIndex >= m_existingRumors.size())
        return;

    const NpcExistingRumor &choice = m_existingRumors.at(choiceIndex);
    const int row = m_rumorTable->rowCount();
    m_rumorTable->insertRow(row);
    m_rumorTable->setItem(row, 0, new QTableWidgetItem(m_rumorKindCombo->currentText().trimmed().isEmpty()
                                                           ? choice.kind
                                                           : m_rumorKindCombo->currentText().trimmed()));
    m_rumorTable->setItem(row, 1, new QTableWidgetItem(choice.stateFrom));
    m_rumorTable->setItem(row, 2, new QTableWidgetItem(choice.stateTo));
    m_rumorTable->setItem(row, 3, new QTableWidgetItem(QString::number(choice.weight)));
    auto *idsItem = new QTableWidgetItem(QString::number(choice.ids));
    idsItem->setData(Qt::UserRole, choice.ids);
    m_rumorTable->setItem(row, 4, idsItem);
    m_rumorTable->setCurrentCell(row, 0);
    refreshSelectedRumorText();
}

void NpcEditorPage::onNewRumor()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create New Rumor"));
    dialog.resize(680, 520);

    auto *layout = new QVBoxLayout(&dialog);
    auto *formGroup = new QGroupBox(tr("Rumor"), &dialog);
    auto *form = new QFormLayout(formGroup);

    auto *kindCombo = new QComboBox(formGroup);
    kindCombo->addItems({QStringLiteral("rumor"), QStringLiteral("rumor_type2")});
    kindCombo->setCurrentText(m_rumorKindCombo ? m_rumorKindCombo->currentText() : QStringLiteral("rumor"));
    auto *fromEdit = new QLineEdit(QStringLiteral("base_0_rank"), formGroup);
    auto *toEdit = new QLineEdit(QStringLiteral("mission_end"), formGroup);
    auto *weightSpin = new QSpinBox(formGroup);
    weightSpin->setRange(0, 999999);
    weightSpin->setValue(1);

    form->addRow(tr("Type:"), kindCombo);
    form->addRow(tr("From State:"), fromEdit);
    form->addRow(tr("To State:"), toEdit);
    form->addRow(tr("Weight:"), weightSpin);
    layout->addWidget(formGroup);

    auto *textEdit = new QPlainTextEdit(&dialog);
    textEdit->setPlaceholderText(tr("Enter rumor text. FLAtlas creates the IDS automatically."));
    textEdit->setMinimumHeight(180);
    layout->addWidget(textEdit, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString kind = kindCombo->currentText().trimmed();
    const QString stateFrom = fromEdit->text().trimmed();
    const QString stateTo = toEdit->text().trimmed();
    int ids = 0;
    const QString rumorText = textEdit->toPlainText().trimmed();
    if (kind.isEmpty() || stateFrom.isEmpty() || stateTo.isEmpty()) {
        QMessageBox::warning(this, tr("Type, from-state, and to-state must be set."), tr("Type, from-state, and to-state must be set."));
        return;
    }
    if (rumorText.isEmpty()) {
        QMessageBox::warning(this, tr("Create Rumor"), tr("Please enter a rumor text."));
        return;
    }

    const IdsDataset dataset = IdsDataService::loadFromGameRoot(m_gameRoot);
    const QString targetDll = IdsDataService::defaultCreationDllName(dataset);
    if (targetDll.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Create Rumor"), tr("No target DLL could be determined for the rumor text."));
        return;
    }
    QString idsError;
    int newGlobalId = 0;
    if (!IdsDataService::writeStringEntry(dataset,
                                          targetDll,
                                          0,
                                          rumorText,
                                          &newGlobalId,
                                          &idsError)) {
        QMessageBox::warning(this,
                             tr("Create Rumor"),
                             tr("Rumor text could not be saved: %1").arg(idsError));
        return;
    }
    ids = newGlobalId;
    m_idsTextByNumber.insert(QString::number(ids), rumorText);

    const int row = m_rumorTable->rowCount();
    m_rumorTable->insertRow(row);
    m_rumorTable->setItem(row, 0, new QTableWidgetItem(kind));
    m_rumorTable->setItem(row, 1, new QTableWidgetItem(stateFrom));
    m_rumorTable->setItem(row, 2, new QTableWidgetItem(stateTo));
    m_rumorTable->setItem(row, 3, new QTableWidgetItem(QString::number(weightSpin->value())));
    auto *idsItem = new QTableWidgetItem(QString::number(ids));
    idsItem->setData(Qt::UserRole, ids);
    m_rumorTable->setItem(row, 4, idsItem);
    m_rumorTable->setCurrentCell(row, 0);

    NpcExistingRumor existing;
    existing.kind = kind;
    existing.stateFrom = stateFrom;
    existing.stateTo = stateTo;
    existing.weight = weightSpin->value();
    existing.ids = ids;
    existing.preview = rumorText.isEmpty() ? resolvedIdsText(ids) : rumorText;
    const bool duplicate = std::any_of(m_existingRumors.begin(), m_existingRumors.end(), [&](const NpcExistingRumor &other) {
        return other.kind == existing.kind && other.stateFrom == existing.stateFrom && other.stateTo == existing.stateTo
               && other.weight == existing.weight && other.ids == existing.ids;
    });
    if (!duplicate)
        m_existingRumors.append(existing);

    refreshSelectedRumorText();
}

void NpcEditorPage::onRemoveRumor()
{
    const int row = m_rumorTable->currentRow();
    if (row >= 0) {
        m_rumorTable->removeRow(row);
        refreshSelectedRumorText();
    }
}

} // namespace flatlas::editors
