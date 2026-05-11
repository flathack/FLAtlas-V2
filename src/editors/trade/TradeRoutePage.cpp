// editors/trade/TradeRoutePage.cpp – Trade-Route-Workspace

#include "TradeRoutePage.h"
#include "TradeScoring.h"

#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/parser/XmlInfocard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QRegularExpression>
#include <QSet>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QToolBar>
#include <QTableWidget>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QSplitter>
#include <QSizePolicy>

#include <QtConcurrent/QtConcurrent>

using namespace flatlas::domain;

namespace flatlas::editors {

namespace {

QString commodityDisplayLabel(const TradeCommodityRecord &commodity, bool useIdsName)
{
    if (!useIdsName)
        return commodity.nickname;
    return commodity.displayName.trimmed().isEmpty() ? commodity.nickname : commodity.displayName.trimmed();
}

QString baseDisplayLabel(const TradeBaseRecord &base, bool useIdsName)
{
    const QString baseName = useIdsName && !base.displayName.trimmed().isEmpty()
        ? base.displayName.trimmed()
        : base.nickname;
    const QString systemName = useIdsName && !base.systemDisplayName.trimmed().isEmpty()
        ? base.systemDisplayName.trimmed()
        : base.systemNickname;
    return QStringLiteral("%1 (%2)").arg(baseName, systemName);
}

QString defaultMsgIdPrefix(const QString &nickname)
{
    return QStringLiteral("gcs_gen_%1").arg(nickname.trimmed());
}

QString gameRootForDataPath(const QString &dataPath)
{
    const QFileInfo dataInfo(dataPath);
    if (dataInfo.fileName().compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0)
        return dataInfo.absolutePath();
    return QDir(dataPath).absolutePath();
}

QString htmlEscape(const QString &text)
{
    return text.toHtmlEscaped();
}

QString htmlList(const QStringList &items)
{
    if (items.isEmpty())
        return QStringLiteral("<p><i>%1</i></p>").arg(htmlEscape(QObject::tr("None")));

    QString html = QStringLiteral("<ul>");
    for (const QString &item : items)
        html += QStringLiteral("<li>%1</li>").arg(htmlEscape(item));
    html += QStringLiteral("</ul>");
    return html;
}

QString htmlSection(const QString &title, const QStringList &items)
{
    return QStringLiteral("<h3 style=\"margin-bottom:4px;\">%1</h3>%2")
        .arg(htmlEscape(title), htmlList(items));
}

QString formatSeconds(int seconds)
{
    const int minutes = seconds / 60;
    const int remainingSeconds = seconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}

class AddCommodityDialog final : public QDialog {
public:
    AddCommodityDialog(const QVector<TradeCommodityRecord> &existingCommodities, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_existingCommodities(existingCommodities)
    {
        setWindowTitle(tr("Add Commodity"));
        resize(680, 720);

        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        for (const auto &commodity : m_existingCommodities)
            m_existingNicknames.insert(commodity.nickname.trimmed().toLower());

        m_nicknameEdit = new QLineEdit(this);
        m_nicknameEdit->setPlaceholderText(QStringLiteral("commodity_new_item"));
        m_nicknameEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9_]+")), m_nicknameEdit));
        form->addRow(tr("Nickname"), m_nicknameEdit);

        m_ingameNameEdit = new QLineEdit(this);
        m_ingameNameEdit->setPlaceholderText(tr("Visible commodity name"));
        form->addRow(tr("Ingame Name"), m_ingameNameEdit);

        m_idsInfoEdit = new QPlainTextEdit(this);
        m_idsInfoEdit->setPlainText(defaultIdsInfoText());
        m_idsInfoEdit->setPlaceholderText(tr("Commodity infocard text"));
        m_idsInfoEdit->setMinimumHeight(96);
        form->addRow(tr("ids_info text"), m_idsInfoEdit);

        m_basePriceSpin = new QSpinBox(this);
        m_basePriceSpin->setRange(0, 100000000);
        m_basePriceSpin->setValue(100);
        form->addRow(tr("Base Price"), m_basePriceSpin);

        m_volumeSpin = new QSpinBox(this);
        m_volumeSpin->setRange(1, 1000000);
        m_volumeSpin->setValue(1);
        form->addRow(tr("Volume"), m_volumeSpin);

        m_msgIdPrefixEdit = new QLineEdit(this);
        form->addRow(tr("msg_id_prefix"), m_msgIdPrefixEdit);

        m_equipmentEdit = new QLineEdit(this);
        form->addRow(tr("equipment"), m_equipmentEdit);

        m_combinableCheck = new QCheckBox(tr("Combinable"), this);
        m_combinableCheck->setChecked(true);
        form->addRow(QString(), m_combinableCheck);

        m_goodSellSpin = createFactorSpin(2.04);
        form->addRow(tr("good_sell_price"), m_goodSellSpin);
        m_badBuySpin = createFactorSpin(2.04);
        form->addRow(tr("bad_buy_price"), m_badBuySpin);
        m_badSellSpin = createFactorSpin(0.84);
        form->addRow(tr("bad_sell_price"), m_badSellSpin);
        m_goodBuySpin = createFactorSpin(0.84);
        form->addRow(tr("good_buy_price"), m_goodBuySpin);

        m_shopArchetypeEdit = new QLineEdit(this);
        m_shopArchetypeEdit->setText(QStringLiteral("Equipment\\models\\commodities\\nn_icons\\cwire_rawmats_2.3db"));
        form->addRow(tr("shop_archetype"), m_shopArchetypeEdit);

        m_itemIconEdit = new QLineEdit(this);
        m_itemIconEdit->setText(QStringLiteral("Equipment\\models\\commodities\\nn_icons\\COMMOD_chemicals.3db"));
        form->addRow(tr("item_icon"), m_itemIconEdit);

        m_jumpDistSpin = new QSpinBox(this);
        m_jumpDistSpin->setRange(0, 64);
        m_jumpDistSpin->setValue(7);
        form->addRow(tr("jump_dist"), m_jumpDistSpin);

        m_hitPtsSpin = new QSpinBox(this);
        m_hitPtsSpin->setRange(1, 100000000);
        m_hitPtsSpin->setValue(250);
        form->addRow(tr("hit_pts"), m_hitPtsSpin);

        m_decayPerSecondSpin = new QDoubleSpinBox(this);
        m_decayPerSecondSpin->setRange(0.0, 1000000.0);
        m_decayPerSecondSpin->setDecimals(6);
        m_decayPerSecondSpin->setSingleStep(0.01);
        m_decayPerSecondSpin->setValue(0.0);
        form->addRow(tr("decay_per_second"), m_decayPerSecondSpin);

        layout->addLayout(form);

        auto *note = new QLabel(tr("This creates a [Good] commodity entry. Add base buy/sell prices afterwards with Add Price."), this);
        note->setWordWrap(true);
        layout->addWidget(note);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
        layout->addWidget(buttons);

        connect(m_nicknameEdit, &QLineEdit::textChanged, this, [this](const QString &value) {
            const QString nickname = value.trimmed();
            m_equipmentEdit->setText(nickname);
            if (!m_msgIdPrefixTouched)
                m_msgIdPrefixEdit->setText(defaultMsgIdPrefix(nickname));
            if (!m_ingameNameTouched)
                m_ingameNameEdit->setText(TradeRouteDataService::fallbackCommodityDisplayName(nickname));
        });
        connect(m_msgIdPrefixEdit, &QLineEdit::textEdited, this, [this]() { m_msgIdPrefixTouched = true; });
        connect(m_ingameNameEdit, &QLineEdit::textEdited, this, [this]() { m_ingameNameTouched = true; });
        connect(buttons, &QDialogButtonBox::accepted, this, &AddCommodityDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        m_nicknameEdit->setText(QStringLiteral("commodity_new_item"));
    }

    TradeCommodityRecord commodity() const
    {
        TradeCommodityRecord commodity;
        commodity.nickname = m_nicknameEdit->text().trimmed();
        commodity.displayName = m_ingameNameEdit->text().trimmed().isEmpty()
            ? TradeRouteDataService::fallbackCommodityDisplayName(commodity.nickname)
            : m_ingameNameEdit->text().trimmed();
        commodity.msgIdPrefix = m_msgIdPrefixEdit->text().trimmed();
        commodity.equipment = m_equipmentEdit->text().trimmed();
        commodity.basePrice = m_basePriceSpin->value();
        commodity.volume = m_volumeSpin->value();
        commodity.idsInfoText = m_idsInfoEdit->toPlainText().trimmed();
        commodity.combinable = m_combinableCheck->isChecked();
        commodity.goodSellPrice = m_goodSellSpin->value();
        commodity.badBuyPrice = m_badBuySpin->value();
        commodity.badSellPrice = m_badSellSpin->value();
        commodity.goodBuyPrice = m_goodBuySpin->value();
        commodity.shopArchetype = m_shopArchetypeEdit->text().trimmed();
        commodity.itemIcon = m_itemIconEdit->text().trimmed();
        commodity.jumpDist = m_jumpDistSpin->value();
        commodity.hitPts = m_hitPtsSpin->value();
        commodity.decayPerSecond = m_decayPerSecondSpin->value();
        return commodity;
    }

    void accept() override
    {
        const QString nickname = m_nicknameEdit->text().trimmed();
        if (nickname.isEmpty()) {
            QMessageBox::warning(this, tr("Add Commodity"), tr("Please enter a commodity nickname."));
            return;
        }
        if (!nickname.startsWith(QStringLiteral("commodity_"), Qt::CaseInsensitive)) {
            QMessageBox::warning(this, tr("Add Commodity"), tr("Commodity nicknames must start with commodity_."));
            return;
        }
        if (m_existingNicknames.contains(nickname.toLower())) {
            QMessageBox::warning(this, tr("Add Commodity"), tr("A commodity with this nickname already exists."));
            return;
        }
        if (m_basePriceSpin->value() <= 0) {
            QMessageBox::warning(this, tr("Add Commodity"), tr("Please enter a base price greater than zero."));
            return;
        }
        QDialog::accept();
    }

private:
    QDoubleSpinBox *createFactorSpin(double value)
    {
        auto *spin = new QDoubleSpinBox(this);
        spin->setRange(0.0, 9999.0);
        spin->setDecimals(4);
        spin->setSingleStep(0.01);
        spin->setValue(value);
        return spin;
    }

    QSet<QString> m_existingNicknames;
    QVector<TradeCommodityRecord> m_existingCommodities;
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_ingameNameEdit = nullptr;
    QPlainTextEdit *m_idsInfoEdit = nullptr;
    QSpinBox *m_basePriceSpin = nullptr;
    QSpinBox *m_volumeSpin = nullptr;
    QLineEdit *m_msgIdPrefixEdit = nullptr;
    QLineEdit *m_equipmentEdit = nullptr;
    QCheckBox *m_combinableCheck = nullptr;
    QDoubleSpinBox *m_goodSellSpin = nullptr;
    QDoubleSpinBox *m_badBuySpin = nullptr;
    QDoubleSpinBox *m_badSellSpin = nullptr;
    QDoubleSpinBox *m_goodBuySpin = nullptr;
    QLineEdit *m_shopArchetypeEdit = nullptr;
    QLineEdit *m_itemIconEdit = nullptr;
    QSpinBox *m_jumpDistSpin = nullptr;
    QSpinBox *m_hitPtsSpin = nullptr;
    QDoubleSpinBox *m_decayPerSecondSpin = nullptr;
    bool m_msgIdPrefixTouched = false;
    bool m_ingameNameTouched = false;

    QString defaultIdsInfoText() const
    {
        for (const auto &commodity : m_existingCommodities) {
            const QString text = commodity.idsInfoText.trimmed();
            if (!text.isEmpty())
                return text;
        }
        return {};
    }
};

class DeleteCommodityDialog final : public QDialog {
public:
    DeleteCommodityDialog(const TradeRouteWorkspaceData &workspace,
                          const QVector<TradeRouteCandidate> &routes,
                          const TradeCommodityRecord &commodity,
                          QWidget *parent = nullptr)
        : QDialog(parent)
        , m_workspace(workspace)
        , m_routes(routes)
        , m_commodity(commodity)
    {
        setWindowTitle(tr("Delete Commodity: %1").arg(m_commodity.nickname));
        resize(820, 620);

        auto *layout = new QVBoxLayout(this);
        auto *intro = new QLabel(
            tr("Delete is blocked until Scan completes. The scan lists goods.ini, select_equip.ini, market entries, generated price rows and current route usage."),
            this);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        m_reportView = new QTextBrowser(this);
        m_reportView->setOpenExternalLinks(false);
        layout->addWidget(m_reportView, 1);

        auto *buttons = new QDialogButtonBox(this);
        m_scanButton = buttons->addButton(tr("Scan"), QDialogButtonBox::ActionRole);
        m_deleteButton = buttons->addButton(tr("Delete Commodity"), QDialogButtonBox::DestructiveRole);
        buttons->addButton(QDialogButtonBox::Cancel);
        m_deleteButton->setEnabled(false);
        layout->addWidget(buttons);

        connect(m_scanButton, &QPushButton::clicked, this, &DeleteCommodityDialog::runScan);
        connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
            runScan();
            if (m_canDelete)
                accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        m_reportView->setHtml(QStringLiteral("<p><i>%1</i></p>").arg(htmlEscape(tr("No scan executed yet."))));
    }

private:
    void runScan()
    {
        m_scanCompleted = true;
        m_canDelete = !m_commodity.nickname.trimmed().isEmpty();
        m_reportView->setHtml(formatReportHtml());
        m_deleteButton->setEnabled(m_scanCompleted && m_canDelete);
    }

    QString formatReportHtml() const
    {
        QString html;
        html += QStringLiteral("<html><body style=\"font-family:'Segoe UI';\">");
        html += QStringLiteral("<h2>%1</h2>")
                    .arg(htmlEscape(commodityDisplayLabel(m_commodity, true)));
        html += QStringLiteral("<p><b>%1</b>: %2</p>")
                    .arg(htmlEscape(tr("Nickname")), htmlEscape(m_commodity.nickname));

        const QString equipmentNickname = m_commodity.equipment.trimmed().isEmpty() ? m_commodity.nickname : m_commodity.equipment.trimmed();

        QStringList goodEntries;
        goodEntries.append(QStringLiteral("%1 | [Good] %2")
                               .arg(m_workspace.goodsFilePath, m_commodity.nickname));
        html += htmlSection(tr("goods.ini Entries To Remove"), goodEntries);

        QStringList selectEntries;
        selectEntries.append(QStringLiteral("%1 | [Commodity] %2")
                                 .arg(m_workspace.selectEquipFilePath, equipmentNickname));
        if (m_commodity.idsName > 0)
            selectEntries.append(QStringLiteral("ids_name %1").arg(m_commodity.idsName));
        if (m_commodity.idsInfo > 0)
            selectEntries.append(QStringLiteral("ids_info %1").arg(m_commodity.idsInfo));
        html += htmlSection(tr("select_equip.ini Entries To Remove"), selectEntries);

        QStringList explicitPrices;
        QStringList implicitPrices;
        for (const auto &price : m_workspace.prices) {
            if (price.commodityNickname.compare(m_commodity.nickname, Qt::CaseInsensitive) != 0)
                continue;
            const QString line = QStringLiteral("%1 | %2 | %3 | %4")
                                     .arg(price.sourceFilePath.isEmpty() ? m_workspace.preferredMarketFilePath : price.sourceFilePath,
                                          price.baseNickname,
                                          price.isSource ? tr("Source") : tr("Sink"),
                                          QString::number(price.price));
            if (price.implicit)
                implicitPrices.append(line);
            else
                explicitPrices.append(line);
        }
        html += htmlSection(tr("MarketGood Entries To Remove"), explicitPrices);
        html += htmlSection(tr("Generated Price Rows To Drop"), implicitPrices);

        QStringList routeLines;
        for (const auto &route : m_routes) {
            if (route.commodity.compare(m_commodity.nickname, Qt::CaseInsensitive) != 0)
                continue;
            routeLines.append(QStringLiteral("%1 -> %2 | %3 total profit")
                                  .arg(route.fromBase, route.toBase)
                                  .arg(route.totalProfit));
        }
        html += htmlSection(tr("Current Routes Affected"), routeLines);

        QStringList result;
        result.append(m_canDelete
                          ? tr("Scan successful. Delete is enabled.")
                          : tr("Scan blocked. Delete stays disabled."));
        result.append(tr("Save All writes the removal to goods.ini, select_equip.ini and market files."));
        html += htmlSection(tr("Result"), result);
        html += QStringLiteral("</body></html>");
        return html;
    }

    TradeRouteWorkspaceData m_workspace;
    QVector<TradeRouteCandidate> m_routes;
    TradeCommodityRecord m_commodity;
    QTextBrowser *m_reportView = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    bool m_scanCompleted = false;
    bool m_canDelete = false;
};

} // namespace

TradeRoutePage::TradeRoutePage(QWidget *parent)
    : QWidget(parent)
    , m_loadWatcher(new QFutureWatcher<TradeRouteWorkspaceData>(this))
    , m_calcWatcher(new QFutureWatcher<QVector<TradeRouteCandidate>>(this))
    , m_recalcTimer(new QTimer(this))
{
    setupUi();

    m_recalcTimer->setInterval(250);
    m_recalcTimer->setSingleShot(true);
    connect(m_recalcTimer, &QTimer::timeout, this, &TradeRoutePage::startRecalculation);
    connect(m_loadWatcher, &QFutureWatcher<TradeRouteWorkspaceData>::finished, this, [this]() {
        m_workspace = m_loadWatcher->result();
        if (m_universe)
            m_workspace.universe = std::shared_ptr<UniverseData>(const_cast<UniverseData *>(m_universe), [](UniverseData *) {});
        populateCommodityFilter();
        populateCommodityTable();
        populatePriceTable();
        scheduleRecalculation();
    });
    connect(m_calcWatcher, &QFutureWatcher<QVector<TradeRouteCandidate>>::finished, this, [this]() {
        m_routes = m_calcWatcher->result();
        populateRouteTable();
        if (!m_routes.isEmpty())
            updateRouteDetails(0);
        else
            updateRouteDetails(-1);
        m_statusLabel->setText(tr("%1 routes ready. %2 commodities, %3 editable price points.")
                                   .arg(m_routes.size())
                                   .arg(m_workspace.commodities.size())
                                   .arg(std::count_if(m_workspace.prices.begin(), m_workspace.prices.end(), [](const TradePriceRecord &price) {
                                       return !price.implicit;
                                   })));
        emit titleChanged(QStringLiteral("Trade Routes (%1)").arg(m_routes.size()));
        emit scanComplete(m_routes.size());
    });
}

void TradeRoutePage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *editorSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    editorSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *commodityBox = new QGroupBox(tr("Commodities"), editorSplitter);
    auto *commodityLayout = new QVBoxLayout(commodityBox);
    m_commodityTable = new QTableWidget(commodityBox);
    m_commodityTable->setColumnCount(6);
    m_commodityTable->setHorizontalHeaderLabels({
        tr("Nickname"), tr("Display"), tr("Base Price"), tr("Volume"), tr("ids_name"), tr("ids_info")
    });
    m_commodityTable->horizontalHeader()->setStretchLastSection(true);
    m_commodityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_commodityTable->setAlternatingRowColors(true);
    m_commodityTable->verticalHeader()->setVisible(false);
    commodityLayout->addWidget(m_commodityTable);

    auto *priceBox = new QGroupBox(tr("Base Prices"), editorSplitter);
    auto *priceLayout = new QVBoxLayout(priceBox);
    m_priceTable = new QTableWidget(priceBox);
    m_priceTable->setColumnCount(5);
    m_priceTable->setHorizontalHeaderLabels({
        tr("Base"), tr("System"), tr("Price"), tr("Role"), tr("State")
    });
    m_priceTable->horizontalHeader()->setStretchLastSection(true);
    m_priceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_priceTable->setAlternatingRowColors(true);
    m_priceTable->verticalHeader()->setVisible(false);
    priceLayout->addWidget(m_priceTable);

    auto *routeBox = new QGroupBox(tr("Routes"), mainSplitter);
    auto *routeLayout = new QVBoxLayout(routeBox);
    m_routeModel = new QStandardItemModel(this);
    m_routeModel->setHorizontalHeaderLabels({
        tr("Commodity"), tr("Buy"), tr("Sell"), tr("Unit Profit"), tr("Total Profit"),
        tr("Jumps"), tr("Time"), tr("Distance"), tr("Profit/Min"), tr("Score")
    });
    m_routeView = new QTableView(routeBox);
    m_routeView->setModel(m_routeModel);
    m_routeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_routeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_routeView->setAlternatingRowColors(true);
    m_routeView->setSortingEnabled(true);
    m_routeView->horizontalHeader()->setStretchLastSection(true);
    m_routeView->verticalHeader()->setVisible(false);
    routeLayout->addWidget(m_routeView);

    auto *detailBox = new QGroupBox(tr("Analysis"), mainSplitter);
    auto *detailLayout = new QVBoxLayout(detailBox);
    m_routeSummaryLabel = new QLabel(detailBox);
    m_routeSummaryLabel->setWordWrap(true);
    detailLayout->addWidget(m_routeSummaryLabel);

    auto *metricsForm = new QFormLayout;
    m_profitLabel = new QLabel(detailBox);
    m_timeLabel = new QLabel(detailBox);
    m_distanceLabel = new QLabel(detailBox);
    m_scoreLabel = new QLabel(detailBox);
    m_pathLabel = new QLabel(detailBox);
    m_pathLabel->setWordWrap(true);
    metricsForm->addRow(tr("Profit"), m_profitLabel);
    metricsForm->addRow(tr("Travel"), m_timeLabel);
    metricsForm->addRow(tr("Distance"), m_distanceLabel);
    metricsForm->addRow(tr("Score"), m_scoreLabel);
    metricsForm->addRow(tr("Path"), m_pathLabel);
    detailLayout->addLayout(metricsForm);

    m_warningList = new QListWidget(detailBox);
    detailLayout->addWidget(m_warningList);

    m_routeScene = new QGraphicsScene(detailBox);
    m_routeViewWidget = new QGraphicsView(m_routeScene, detailBox);
    m_routeViewWidget->setMinimumHeight(180);
    detailLayout->addWidget(m_routeViewWidget);

    m_segmentTable = new QTableWidget(detailBox);
    m_segmentTable->setColumnCount(5);
    m_segmentTable->setHorizontalHeaderLabels({
        tr("Type"), tr("System"), tr("From"), tr("To"), tr("Seconds")
    });
    m_segmentTable->horizontalHeader()->setStretchLastSection(true);
    m_segmentTable->verticalHeader()->setVisible(false);
    m_segmentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailLayout->addWidget(m_segmentTable);

    mainSplitter->addWidget(editorSplitter);
    mainSplitter->addWidget(routeBox);
    mainSplitter->addWidget(detailBox);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 3);
    mainSplitter->setStretchFactor(2, 2);
    editorSplitter->setStretchFactor(0, 1);
    editorSplitter->setStretchFactor(1, 2);
    layout->addWidget(mainSplitter, 1);

    m_statusLabel = new QLabel(tr("Ready. Load Freelancer data to build the trade workspace."), this);
    layout->addWidget(m_statusLabel);

    connect(m_commodityTable, &QTableWidget::itemSelectionChanged, this, &TradeRoutePage::populatePriceTable);
    connect(m_commodityTable, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (m_updatingTables || row < 0 || row >= m_workspace.commodities.size())
            return;
        auto &commodity = m_workspace.commodities[row];
        const QString text = m_commodityTable->item(row, column) ? m_commodityTable->item(row, column)->text().trimmed() : QString();
        switch (column) {
        case 0: commodity.nickname = text; break;
        case 1: commodity.displayName = text; break;
        case 2: commodity.basePrice = qMax(0, text.toInt()); break;
        case 3: commodity.volume = qMax(1, text.toInt()); break;
        case 4: commodity.idsName = qMax(0, text.toInt()); break;
        case 5: commodity.idsInfo = qMax(0, text.toInt()); break;
        default: break;
        }
        for (auto &price : m_workspace.prices) {
            if (price.commodityNickname.compare(commodity.nickname, Qt::CaseInsensitive) == 0 && price.implicit) {
                price.price = commodity.basePrice;
                price.multiplier = 1.0;
            }
        }
        markDirty();
        populateCommodityFilter();
        populatePriceTable();
        scheduleRecalculation();
    });
    connect(m_priceTable, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (m_updatingTables)
            return;
        const QString commodityNickname = selectedCommodityNickname();
        if (commodityNickname.isEmpty())
            return;

        int visibleIndex = -1;
        for (int i = 0; i < m_workspace.prices.size(); ++i) {
            const auto &price = m_workspace.prices.at(i);
            if (price.commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) != 0)
                continue;
            if (visibleIndex == row) {
                if (price.implicit)
                    return;
                auto &editablePrice = m_workspace.prices[i];
                if (column == 2) {
                    editablePrice.price = qMax(0, m_priceTable->item(row, column)->text().toInt());
                    const auto commodityIt = std::find_if(m_workspace.commodities.begin(), m_workspace.commodities.end(), [commodityNickname](const TradeCommodityRecord &commodity) {
                        return commodity.nickname.compare(commodityNickname, Qt::CaseInsensitive) == 0;
                    });
                    if (commodityIt != m_workspace.commodities.end() && commodityIt->basePrice > 0)
                        editablePrice.multiplier = static_cast<double>(editablePrice.price) / static_cast<double>(commodityIt->basePrice);
                } else if (column == 3) {
                    editablePrice.isSource = m_priceTable->item(row, column)->text().trimmed().compare(QStringLiteral("Source"), Qt::CaseInsensitive) == 0;
                }
                markDirty();
                scheduleRecalculation();
                return;
            }
            ++visibleIndex;
        }
    });
    connect(m_routeView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex &current) {
        updateRouteDetails(current.row());
    });
}

void TradeRoutePage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(16, 16));

    m_toolBar->addAction(tr("Reload"), this, &TradeRoutePage::reloadWorkspace);
    m_toolBar->addAction(tr("Save All"), this, &TradeRoutePage::saveWorkspace);
    m_toolBar->addAction(tr("Recalculate"), this, &TradeRoutePage::scanAndCalculate);
    m_toolBar->addSeparator();
    m_toolBar->addAction(tr("Add Commodity"), this, &TradeRoutePage::addCommodity);
    m_toolBar->addAction(tr("Remove Commodity"), this, &TradeRoutePage::removeCommodity);
    m_toolBar->addAction(tr("Add Price"), this, &TradeRoutePage::addPriceEntry);
    m_toolBar->addAction(tr("Remove Price"), this, &TradeRoutePage::removePriceEntry);

    m_toolBar->addSeparator();
    m_toolBar->addWidget(new QLabel(tr(" Search: "), m_toolBar));
    m_searchEdit = new QLineEdit(m_toolBar);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("commodity, base, system"));
    m_toolBar->addWidget(m_searchEdit);

    m_toolBar->addWidget(new QLabel(tr(" Commodity: "), m_toolBar));
    m_commodityFilter = new QComboBox(m_toolBar);
    m_toolBar->addWidget(m_commodityFilter);

    m_toolBar->addWidget(new QLabel(tr(" Cargo: "), m_toolBar));
    m_cargoSpin = new QSpinBox(m_toolBar);
    m_cargoSpin->setRange(1, 10000);
    m_cargoSpin->setValue(275);
    m_toolBar->addWidget(m_cargoSpin);

    m_toolBar->addWidget(new QLabel(tr(" Max jumps: "), m_toolBar));
    m_maxJumpsSpin = new QSpinBox(m_toolBar);
    m_maxJumpsSpin->setRange(0, 64);
    m_maxJumpsSpin->setValue(12);
    m_toolBar->addWidget(m_maxJumpsSpin);

    m_maxResultsSpin = new QSpinBox(m_toolBar);
    m_maxResultsSpin->setRange(10, 1000);
    m_maxResultsSpin->setValue(250);
    m_toolBar->addWidget(new QLabel(tr(" Max routes: "), m_toolBar));
    m_toolBar->addWidget(m_maxResultsSpin);

    m_localOnlyCheck = new QCheckBox(tr("Local only"), m_toolBar);
    m_toolBar->addWidget(m_localOnlyCheck);

    m_useIdsNameCheck = new QCheckBox(tr("use ids_name"), m_toolBar);
    m_useIdsNameCheck->setChecked(true);
    m_useIdsNameCheck->setToolTip(tr("Use resolved ids_name ingame names instead of nicknames."));
    m_toolBar->addWidget(m_useIdsNameCheck);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &TradeRoutePage::scheduleRecalculation);
    connect(m_commodityFilter, &QComboBox::currentTextChanged, this, &TradeRoutePage::scheduleRecalculation);
    connect(m_cargoSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TradeRoutePage::scheduleRecalculation);
    connect(m_maxJumpsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TradeRoutePage::scheduleRecalculation);
    connect(m_maxResultsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TradeRoutePage::scheduleRecalculation);
    connect(m_localOnlyCheck, &QCheckBox::checkStateChanged, this, &TradeRoutePage::scheduleRecalculation);
    connect(m_useIdsNameCheck, &QCheckBox::checkStateChanged, this, [this]() {
        populateCommodityFilter();
        populateCommodityTable();
        populatePriceTable();
        scheduleRecalculation();
    });
}

void TradeRoutePage::setDataPath(const QString &dataPath)
{
    m_dataPath = dataPath;
}

void TradeRoutePage::setUniverseData(const UniverseData *universe)
{
    m_universe = universe;
}

void TradeRoutePage::setBaseSystemMap(const QHash<QString, QString> &map)
{
    m_baseToSystem = map;
}

void TradeRoutePage::scanAndCalculate()
{
    if (m_dataPath.isEmpty()) {
        m_statusLabel->setText(tr("No data path set."));
        return;
    }

    if (m_workspace.dataPath.compare(m_dataPath, Qt::CaseInsensitive) != 0 && !m_loadWatcher->isRunning()) {
        reloadWorkspace();
        return;
    }

    scheduleRecalculation();
}

void TradeRoutePage::reloadWorkspace()
{
    if (m_dataPath.isEmpty()) {
        m_statusLabel->setText(tr("No data path set."));
        return;
    }

    m_statusLabel->setText(tr("Loading commodities, markets and universe graph..."));
    const QString dataPath = m_dataPath;
    m_loadWatcher->setFuture(QtConcurrent::run([dataPath]() {
        return TradeRouteDataService::loadFromDataPath(dataPath);
    }));
}

void TradeRoutePage::saveWorkspace()
{
    QString errorMessage;
    if (!TradeRouteDataService::saveWorkspace(m_workspace, &errorMessage)) {
        QMessageBox::warning(this, tr("Trade Route Editor"), errorMessage);
        return;
    }

    m_dirty = false;
    m_statusLabel->setText(tr("Trade data saved to goods.ini, select_equip.ini and market_commodities.ini."));
    reloadWorkspace();
}

void TradeRoutePage::scheduleRecalculation()
{
    if (!m_workspace.dataPath.isEmpty())
        m_recalcTimer->start();
}

void TradeRoutePage::startRecalculation()
{
    if (m_workspace.dataPath.isEmpty())
        return;

    TradeRouteWorkspaceData workspace = workspaceForCurrentNameMode();

    TradeRouteFilter filter;
    filter.searchText = m_searchEdit->text().trimmed();
    filter.commodityNickname = m_commodityFilter->currentData().toString();
    filter.cargoCapacity = m_cargoSpin->value();
    filter.maxJumps = m_maxJumpsSpin->value();
    filter.maxResults = m_maxResultsSpin->value();
    filter.localRoutesOnly = m_localOnlyCheck->isChecked();

    m_statusLabel->setText(tr("Recalculating trade routes..."));
    m_calcWatcher->setFuture(QtConcurrent::run([workspace, filter]() {
        TradeScoring scoring;
        scoring.setWorkspaceData(&workspace);
        return scoring.calculateRoutes(filter);
    }));
}

void TradeRoutePage::populateCommodityTable()
{
    m_updatingTables = true;
    m_commodityTable->setRowCount(m_workspace.commodities.size());
    for (int row = 0; row < m_workspace.commodities.size(); ++row) {
        const auto &commodity = m_workspace.commodities.at(row);
        m_commodityTable->setItem(row, 0, new QTableWidgetItem(commodity.nickname));
        m_commodityTable->setItem(row, 1, new QTableWidgetItem(commodityDisplayLabel(commodity, m_useIdsNameCheck->isChecked())));
        m_commodityTable->setItem(row, 2, new QTableWidgetItem(QString::number(commodity.basePrice)));
        m_commodityTable->setItem(row, 3, new QTableWidgetItem(QString::number(commodity.volume)));
        m_commodityTable->setItem(row, 4, new QTableWidgetItem(QString::number(commodity.idsName)));
        m_commodityTable->setItem(row, 5, new QTableWidgetItem(QString::number(commodity.idsInfo)));
    }
    m_commodityTable->resizeColumnsToContents();
    m_updatingTables = false;
    if (m_commodityTable->rowCount() > 0 && m_commodityTable->currentRow() < 0)
        m_commodityTable->selectRow(0);
}

void TradeRoutePage::populatePriceTable()
{
    m_updatingTables = true;
    const QString commodityNickname = selectedCommodityNickname();
    QVector<TradePriceRecord> prices;
    for (const auto &price : m_workspace.prices) {
        if (commodityNickname.isEmpty() || price.commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) == 0)
            prices.append(price);
    }

    m_priceTable->setRowCount(prices.size());
    QHash<QString, QString> systemDisplayByBase;
    for (const auto &base : m_workspace.bases) {
        const QString systemName = m_useIdsNameCheck->isChecked() && !base.systemDisplayName.trimmed().isEmpty()
            ? base.systemDisplayName.trimmed()
            : base.systemNickname;
        systemDisplayByBase.insert(base.nickname.trimmed().toLower(), systemName);
    }
    for (int row = 0; row < prices.size(); ++row) {
        const auto &price = prices.at(row);
        auto *baseItem = new QTableWidgetItem(m_useIdsNameCheck->isChecked() && !price.baseDisplayName.trimmed().isEmpty()
                                                  ? price.baseDisplayName.trimmed()
                                                  : price.baseNickname);
        baseItem->setFlags(baseItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 0, baseItem);

        auto *systemItem = new QTableWidgetItem(systemDisplayByBase.value(price.baseNickname.trimmed().toLower(),
                                                                          price.systemNickname));
        systemItem->setFlags(systemItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 1, systemItem);

        auto *priceItem = new QTableWidgetItem(QString::number(price.price));
        if (price.implicit)
            priceItem->setFlags(priceItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 2, priceItem);

        auto *roleItem = new QTableWidgetItem(price.isSource ? tr("Source") : tr("Sink"));
        if (price.implicit)
            roleItem->setFlags(roleItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 3, roleItem);

        auto *stateItem = new QTableWidgetItem(price.implicit ? tr("Implicit") : tr("Explicit"));
        stateItem->setFlags(stateItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 4, stateItem);
    }
    m_priceTable->resizeColumnsToContents();
    m_updatingTables = false;
}

void TradeRoutePage::populateRouteTable()
{
    m_routeModel->removeRows(0, m_routeModel->rowCount());
    for (int index = 0; index < m_routes.size(); ++index) {
        const auto &route = m_routes.at(index);
        QList<QStandardItem *> row;
        row.append(new QStandardItem(route.commodityDisplayName.isEmpty() ? route.commodity : route.commodityDisplayName));
        row.append(new QStandardItem(route.fromBase));
        row.append(new QStandardItem(route.toBase));
        row.append(new QStandardItem(QString::number(route.profit)));
        row.append(new QStandardItem(QString::number(route.totalProfit)));
        row.append(new QStandardItem(QString::number(route.jumps)));
        row.append(new QStandardItem(formatSeconds(route.travelTimeSeconds)));
        row.append(new QStandardItem(QString::number(route.totalDistance, 'f', 0)));
        row.append(new QStandardItem(QString::number(route.profitPerMinute, 'f', 1)));
        row.append(new QStandardItem(QString::number(route.score, 'f', 1)));
        for (auto *item : row)
            item->setData(index, Qt::UserRole);
        m_routeModel->appendRow(row);
    }
    m_routeView->resizeColumnsToContents();
    if (m_routeModel->rowCount() > 0)
        m_routeView->selectRow(0);
}

void TradeRoutePage::populateCommodityFilter()
{
    const QString currentValue = m_commodityFilter->currentData().toString();
    m_commodityFilter->blockSignals(true);
    m_commodityFilter->clear();
    m_commodityFilter->addItem(tr("All commodities"), QString());
    for (const auto &commodity : m_workspace.commodities)
        m_commodityFilter->addItem(commodityDisplayLabel(commodity, m_useIdsNameCheck->isChecked()), commodity.nickname);
    const int currentIndex = m_commodityFilter->findData(currentValue);
    if (currentIndex >= 0)
        m_commodityFilter->setCurrentIndex(currentIndex);
    m_commodityFilter->blockSignals(false);
}

void TradeRoutePage::updateRouteDetails(int routeIndex)
{
    if (routeIndex < 0 || routeIndex >= m_routes.size()) {
        m_routeSummaryLabel->setText(tr("No route selected."));
        m_profitLabel->clear();
        m_timeLabel->clear();
        m_distanceLabel->clear();
        m_scoreLabel->clear();
        m_pathLabel->clear();
        m_warningList->clear();
        m_segmentTable->setRowCount(0);
        m_routeScene->clear();
        return;
    }

    const auto &route = m_routes.at(routeIndex);
    m_routeSummaryLabel->setText(tr("%1 from %2 to %3")
                                     .arg(route.commodityDisplayName.isEmpty() ? route.commodity : route.commodityDisplayName,
                                          route.fromBase,
                                          route.toBase));
    m_profitLabel->setText(tr("%1 / unit, %2 total").arg(route.profit).arg(route.totalProfit));
    m_timeLabel->setText(tr("%1 (%2 per minute)").arg(formatSeconds(route.travelTimeSeconds), QString::number(route.profitPerMinute, 'f', 1)));
    m_distanceLabel->setText(tr("%1 units (%2 per distance)").arg(QString::number(route.totalDistance, 'f', 0), QString::number(route.profitPerDistance, 'f', 4)));
    m_scoreLabel->setText(tr("%1 score, risk %2, plausibility delta %3")
                              .arg(QString::number(route.score, 'f', 1),
                                   QString::number(route.riskScore, 'f', 1),
                                   QString::number(route.plausibilityDelta, 'f', 1)));
    m_pathLabel->setText(route.pathSystemNames.join(QStringLiteral(" -> ")));

    m_warningList->clear();
    if (route.warnings.isEmpty())
        m_warningList->addItem(tr("No plausibility warnings."));
    else
        m_warningList->addItems(route.warnings);

    m_segmentTable->setRowCount(route.segments.size());
    for (int row = 0; row < route.segments.size(); ++row) {
        const auto &segment = route.segments.at(row);
        m_segmentTable->setItem(row, 0, new QTableWidgetItem(segment.type));
        m_segmentTable->setItem(row, 1, new QTableWidgetItem(segment.systemDisplayName));
        m_segmentTable->setItem(row, 2, new QTableWidgetItem(segment.fromLabel));
        m_segmentTable->setItem(row, 3, new QTableWidgetItem(segment.toLabel));
        m_segmentTable->setItem(row, 4, new QTableWidgetItem(QString::number(segment.seconds)));
    }
    m_segmentTable->resizeColumnsToContents();
    refreshRouteScene(route);
}

void TradeRoutePage::refreshRouteScene(const TradeRouteCandidate &route)
{
    m_routeScene->clear();
    if (route.pathSystemNames.isEmpty())
        return;

    const int spacing = 150;
    const int y = 70;
    for (int i = 0; i < route.pathSystemNames.size(); ++i) {
        const int x = 30 + (i * spacing);
        m_routeScene->addEllipse(x, y, 22, 22, QPen(Qt::black), QBrush(QColor(QStringLiteral("#68a1ff"))));
        auto *label = m_routeScene->addText(route.pathSystemNames.at(i));
        label->setPos(x - 15, y + 28);
        if (i < route.pathSystemNames.size() - 1) {
            QPainterPath path;
            path.moveTo(x + 22, y + 11);
            path.lineTo(x + spacing - 8, y + 11);
            m_routeScene->addPath(path, QPen(QColor(QStringLiteral("#1f2937")), 2));
        }
    }
    m_routeScene->setSceneRect(m_routeScene->itemsBoundingRect().adjusted(-20, -20, 20, 20));
}

void TradeRoutePage::markDirty()
{
    m_dirty = true;
}

QString TradeRoutePage::selectedCommodityNickname() const
{
    const int row = m_commodityTable->currentRow();
    if (row < 0 || row >= m_workspace.commodities.size())
        return {};
    return m_workspace.commodities.at(row).nickname;
}

int TradeRoutePage::selectedRouteIndex() const
{
    const QModelIndex index = m_routeView->currentIndex();
    if (!index.isValid())
        return -1;
    return m_routeModel->item(index.row(), 0)->data(Qt::UserRole).toInt();
}

void TradeRoutePage::addCommodity()
{
    AddCommodityDialog dialog(m_workspace.commodities, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    TradeCommodityRecord commodity = dialog.commodity();
    const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRootForDataPath(m_dataPath));
    const QString targetDll = flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
    QString idsError;
    int idsName = 0;
    if (!flatlas::infrastructure::IdsDataService::writeStringEntry(
            dataset, targetDll, 0, commodity.displayName.trimmed(), &idsName, &idsError)) {
        QMessageBox::warning(this,
                             tr("Add Commodity"),
                             tr("The commodity name could not be written to the IDS data.\n%1").arg(idsError));
        return;
    }

    const QString infoText = commodity.idsInfoText.trimmed().isEmpty()
        ? commodity.displayName.trimmed()
        : commodity.idsInfoText.trimmed();
    int idsInfo = 0;
    if (!flatlas::infrastructure::IdsDataService::writeInfocardEntry(
            dataset,
            targetDll,
            0,
            flatlas::infrastructure::XmlInfocard::wrapAsInfocard(infoText),
            &idsInfo,
            &idsError)) {
        QMessageBox::warning(this,
                             tr("Add Commodity"),
                             tr("The commodity infocard could not be written to the IDS data.\n%1").arg(idsError));
        return;
    }

    commodity.idsName = idsName;
    commodity.idsInfo = idsInfo;
    commodity.idsInfoText = infoText;
    commodity.sourceFilePath = m_workspace.goodsFilePath;
    m_workspace.commodities.append(commodity);
    populateCommodityFilter();
    populateCommodityTable();
    markDirty();
    scheduleRecalculation();
}

void TradeRoutePage::removeCommodity()
{
    const int row = m_commodityTable->currentRow();
    if (row < 0 || row >= m_workspace.commodities.size())
        return;
    const TradeCommodityRecord commodity = m_workspace.commodities.at(row);
    const QString commodityNickname = commodity.nickname;

    DeleteCommodityDialog dialog(m_workspace, m_routes, commodity, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_workspace.commodities.removeAt(row);
    m_workspace.prices.erase(std::remove_if(m_workspace.prices.begin(), m_workspace.prices.end(), [&commodityNickname](const TradePriceRecord &price) {
        return price.commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) == 0;
    }), m_workspace.prices.end());
    populateCommodityFilter();
    populateCommodityTable();
    populatePriceTable();
    markDirty();
    scheduleRecalculation();
}

void TradeRoutePage::addPriceEntry()
{
    const QString commodityNickname = selectedCommodityNickname();
    if (commodityNickname.isEmpty())
        return;

    QStringList choices;
    for (const auto &base : m_workspace.bases)
        choices.append(baseDisplayLabel(base, m_useIdsNameCheck->isChecked()));
    bool ok = false;
    const QString chosen = QInputDialog::getItem(this, tr("Add Price Entry"), tr("Base"), choices, 0, false, &ok);
    if (!ok || chosen.isEmpty())
        return;

    const int selectedIndex = choices.indexOf(chosen);
    if (selectedIndex < 0 || selectedIndex >= m_workspace.bases.size())
        return;

    const auto &base = m_workspace.bases.at(selectedIndex);
    const int price = QInputDialog::getInt(this, tr("Add Price Entry"), tr("Actual price"), 100, 1, 100000000, 1, &ok);
    if (!ok)
        return;

    const auto commodityIt = std::find_if(m_workspace.commodities.begin(), m_workspace.commodities.end(), [commodityNickname](const TradeCommodityRecord &commodity) {
        return commodity.nickname.compare(commodityNickname, Qt::CaseInsensitive) == 0;
    });
    if (commodityIt == m_workspace.commodities.end())
        return;

    TradePriceRecord record;
    record.baseNickname = base.nickname;
    record.baseDisplayName = base.displayName;
    record.systemNickname = base.systemNickname;
    record.commodityNickname = commodityNickname;
    record.price = price;
    record.multiplier = commodityIt->basePrice > 0 ? static_cast<double>(price) / static_cast<double>(commodityIt->basePrice) : 1.0;
    record.isSource = QMessageBox::question(this,
                                            tr("Price Role"),
                                            tr("Should this base be treated as a source (seller) for the commodity?"),
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::Yes) == QMessageBox::Yes;
    record.implicit = false;
    record.sourceFilePath = m_workspace.preferredMarketFilePath;
    m_workspace.prices.append(record);
    populatePriceTable();
    markDirty();
    scheduleRecalculation();
}

void TradeRoutePage::removePriceEntry()
{
    const QString commodityNickname = selectedCommodityNickname();
    const int row = m_priceTable->currentRow();
    if (commodityNickname.isEmpty() || row < 0)
        return;

    int visibleIndex = -1;
    for (int i = 0; i < m_workspace.prices.size(); ++i) {
        if (m_workspace.prices.at(i).commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) != 0)
            continue;
        ++visibleIndex;
        if (visibleIndex == row) {
            if (m_workspace.prices.at(i).implicit)
                return;
            m_workspace.prices.removeAt(i);
            populatePriceTable();
            markDirty();
            scheduleRecalculation();
            return;
        }
    }
}

TradeRouteWorkspaceData TradeRoutePage::workspaceForCurrentNameMode() const
{
    TradeRouteWorkspaceData workspace = m_workspace;
    if (m_universe)
        workspace.universe = std::make_shared<UniverseData>(*m_universe);
    else if (workspace.universe)
        workspace.universe = std::make_shared<UniverseData>(*workspace.universe);

    if (m_useIdsNameCheck && m_useIdsNameCheck->isChecked())
        return workspace;

    if (workspace.universe) {
        for (auto &system : workspace.universe->systems)
            system.displayName = system.nickname;
    }
    for (auto &commodity : workspace.commodities)
        commodity.displayName = commodity.nickname;
    for (auto &base : workspace.bases) {
        base.displayName = base.nickname;
        base.systemDisplayName = base.systemNickname;
    }
    for (auto &jump : workspace.jumps)
        jump.objectDisplayName = jump.objectNickname;
    for (auto &price : workspace.prices)
        price.baseDisplayName = price.baseNickname;
    return workspace;
}

} // namespace flatlas::editors
