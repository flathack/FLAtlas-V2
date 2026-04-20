#include "HelpBrowser.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTextBrowser>

namespace flatlas::tools {

HelpBrowser::HelpBrowser(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("FLAtlas Help"));
    setMinimumSize(800, 500);
    resize(900, 600);

    buildUi();
    loadBuiltinTopics();
}

void HelpBrowser::buildUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_topicList = new QListWidget;
    m_topicList->setMaximumWidth(250);
    m_topicList->setMinimumWidth(150);

    m_browser = new QTextBrowser;
    m_browser->setOpenExternalLinks(true);

    m_splitter->addWidget(m_topicList);
    m_splitter->addWidget(m_browser);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_splitter);

    connect(m_topicList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        const QString id = m_topicList->item(row)->data(Qt::UserRole).toString();
        if (m_topics.contains(id))
            m_browser->setHtml(m_topics.value(id).html);
    });
}

void HelpBrowser::showTopic(const QString &topicId)
{
    if (m_topics.contains(topicId)) {
        m_browser->setHtml(m_topics.value(topicId).html);
        // Selektiere das Thema in der Liste
        for (int i = 0; i < m_topicList->count(); ++i) {
            if (m_topicList->item(i)->data(Qt::UserRole).toString() == topicId) {
                m_topicList->setCurrentRow(i);
                break;
            }
        }
    }
    show();
    raise();
    activateWindow();
}

void HelpBrowser::registerTopic(const HelpTopic &topic)
{
    m_topics.insert(topic.id, topic);

    auto *item = new QListWidgetItem(topic.title);
    item->setData(Qt::UserRole, topic.id);
    m_topicList->addItem(item);
}

QStringList HelpBrowser::topicIds() const
{
    return m_topics.keys();
}

QString HelpBrowser::topicForContext(const QString &contextId)
{
    // Mapping von Kontext (Editor/Widget-Klasse) zu Hilfe-Thema
    static const QMap<QString, QString> mapping = {
        {QStringLiteral("SystemEditorPage"),    QStringLiteral("system-editor")},
        {QStringLiteral("UniverseEditorPage"),  QStringLiteral("universe-editor")},
        {QStringLiteral("BaseEditorPage"),      QStringLiteral("base-editor")},
        {QStringLiteral("IniEditorPage"),       QStringLiteral("ini-editor")},
        {QStringLiteral("TradeRoutePage"),      QStringLiteral("trade-routes")},
        {QStringLiteral("IdsEditorPage"),       QStringLiteral("ids-editor")},
        {QStringLiteral("ModManagerPage"),      QStringLiteral("mod-manager")},
        {QStringLiteral("NpcEditorPage"),       QStringLiteral("npc-editor")},
        {QStringLiteral("InfocardEditor"),      QStringLiteral("infocard-editor")},
        {QStringLiteral("NewsRumorEditor"),     QStringLiteral("news-rumor-editor")},
        {QStringLiteral("ModelPreview"),        QStringLiteral("model-viewer")},
        {QStringLiteral("JumpConnectionDialog"),QStringLiteral("jump-connections")},
    };
    return mapping.value(contextId, QStringLiteral("overview"));
}

void HelpBrowser::loadBuiltinTopics()
{
    registerTopic({
        QStringLiteral("overview"),
        tr("Overview"),
        tr("<h1>FLAtlas V2</h1>"
           "<p>FLAtlas is a comprehensive editor and toolkit for Freelancer game data.</p>"
           "<h2>Getting Started</h2>"
           "<ul>"
           "<li>Use <b>File → Open System</b> to load a .ini system file</li>"
           "<li>Use <b>File → Open Universe</b> to load the universe map</li>"
           "<li>Press <b>F1</b> in any editor for context-sensitive help</li>"
           "</ul>"
           "<h2>Features</h2>"
           "<ul>"
           "<li>System Editor – Edit star systems, objects, zones</li>"
           "<li>Universe Editor – Manage the game universe</li>"
           "<li>INI Editor – Syntax-highlighted INI file editing</li>"
           "<li>Trade Route Analyzer – Find profitable trade routes</li>"
           "<li>3D Model Viewer – Preview ship and station models</li>"
           "<li>Mod Manager – Manage game modifications</li>"
           "</ul>")
    });

    registerTopic({
        QStringLiteral("system-editor"),
        tr("System Editor"),
        tr("<h1>System Editor</h1>"
           "<p>The System Editor allows you to edit Freelancer star systems.</p>"
           "<h2>Loading a System</h2>"
           "<p>Use <b>File → Open System</b> or double-click a system in the Universe Editor.</p>"
           "<h2>Editing Objects</h2>"
           "<p>Select objects in the 2D map or the object list. Properties appear in the right panel.</p>"
           "<h2>Creating Objects</h2>"
           "<p>Use the toolbar or right-click menu to add new objects to the system.</p>")
    });

    registerTopic({
        QStringLiteral("universe-editor"),
        tr("Universe Editor"),
        tr("<h1>Universe Editor</h1>"
           "<p>The Universe Editor shows all star systems and their connections.</p>"
           "<h2>Navigation</h2>"
           "<p>Scroll to zoom, drag to pan. Double-click a system to open it in the System Editor.</p>")
    });

    registerTopic({
        QStringLiteral("base-editor"),
        tr("Base Editor"),
        tr("<h1>Base Editor</h1>"
           "<p>Edit space stations and planet bases: rooms, NPCs, equipment dealers.</p>")
    });

    registerTopic({
        QStringLiteral("ini-editor"),
        tr("INI Editor"),
        tr("<h1>INI Editor</h1>"
           "<p>A syntax-highlighted editor for Freelancer INI configuration files.</p>"
           "<h2>Features</h2>"
           "<ul>"
           "<li>Syntax highlighting for sections, keys, values</li>"
           "<li>Auto-completion for known keys</li>"
           "<li>Error detection for invalid entries</li>"
           "</ul>")
    });

    registerTopic({
        QStringLiteral("trade-routes"),
        tr("Trade Routes"),
        tr("<h1>Trade Route Analyzer</h1>"
           "<p>Scan commodity markets and find the most profitable trade routes.</p>"
           "<h2>Usage</h2>"
           "<p>Load market data, then click <b>Analyze</b> to compute optimal routes.</p>")
    });

    registerTopic({
        QStringLiteral("ids-editor"),
        tr("IDS Editor"),
        tr("<h1>IDS String Editor</h1>"
           "<p>Edit string resources (IDS) used for in-game text, names, and descriptions.</p>")
    });

    registerTopic({
        QStringLiteral("mod-manager"),
        tr("Mod Manager"),
        tr("<h1>Mod Manager</h1>"
           "<p>Manage Freelancer modifications: enable, disable, and configure mods.</p>")
    });

    registerTopic({
        QStringLiteral("npc-editor"),
        tr("NPC Editor"),
        tr("<h1>NPC Editor</h1>"
           "<p>Create and edit NPCs, their factions, loadouts, and patrol routes.</p>")
    });

    registerTopic({
        QStringLiteral("infocard-editor"),
        tr("Infocard Editor"),
        tr("<h1>Infocard Editor</h1>"
           "<p>Edit the XML infocards that contain rich-text descriptions in Freelancer.</p>")
    });

    registerTopic({
        QStringLiteral("news-rumor-editor"),
        tr("News & Rumor Editor"),
        tr("<h1>News & Rumor Editor</h1>"
           "<p>Edit bar news and rumors displayed at bases.</p>")
    });

    registerTopic({
        QStringLiteral("model-viewer"),
        tr("Model Viewer"),
        tr("<h1>3D Model Viewer</h1>"
           "<p>Preview CMP/3DB models with textures. Orbit camera with mouse.</p>")
    });

    registerTopic({
        QStringLiteral("jump-connections"),
        tr("Jump Connections"),
        tr("<h1>Jump Connection Editor</h1>"
           "<p>Create and manage jump gate/hole connections between star systems.</p>")
    });
}

} // namespace flatlas::tools
