#include "WelcomePage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QDesktopServices>
#include <QUrl>
#include "core/Config.h"
#include "core/Theme.h"
#include "core/I18n.h"

namespace flatlas::ui {

WelcomePage::WelcomePage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    // --- Title ---
    auto *title = new QLabel(QStringLiteral("<h1>Welcome to FL Atlas</h1>"), this);
    layout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Welcome back. Use this page as a quick start and entry point to Mod Manager."), this);
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    // --- Quick Introduction ---
    auto *introHeader = new QLabel(QStringLiteral("<span style='color:#5599cc;'>Quick Introduction</span>"), this);
    layout->addWidget(introHeader);

    auto *introFrame = new QFrame(this);
    introFrame->setFrameStyle(QFrame::StyledPanel);
    auto *introLayout = new QVBoxLayout(introFrame);
    introLayout->setContentsMargins(12, 10, 12, 10);
    introLayout->setSpacing(6);

    auto *introText = new QLabel(
        tr("FL Atlas is a visual editor for Freelancer INI files. You can edit systems, "
           "objects, zones, trade routes and IDS names in one place."), this);
    introText->setWordWrap(true);
    introLayout->addWidget(introText);

    auto *howToStart = new QLabel(QStringLiteral("<b>How to start</b>"), this);
    introLayout->addWidget(howToStart);

    auto *steps = new QLabel(
        tr("1. Open the <b>Mod Manager</b>.\n"
           "2. Create a mod or select an existing mod folder.\n"
           "3. Set that mod as the <b>editing context</b>.\n"
           "4. Then switch to Universe, System, Trade Routes, or Name Editor."), this);
    steps->setTextFormat(Qt::RichText);
    steps->setWordWrap(true);
    steps->setText(
        tr("<ol style='margin:0; padding-left:18px;'>"
           "<li>Open the <b>Mod Manager</b>.</li>"
           "<li>Create a mod or select an existing mod folder.</li>"
           "<li>Set that mod as the <b>editing context</b>.</li>"
           "<li>Then switch to Universe, System, Trade Routes, or Name Editor.</li>"
           "</ol>"));
    introLayout->addWidget(steps);
    layout->addWidget(introFrame);

    // --- Quick Start ---
    auto *quickStartHeader = new QLabel(QStringLiteral("<span style='color:#5599cc;'>Quick Start</span>"), this);
    layout->addWidget(quickStartHeader);

    auto *quickStartFrame = new QFrame(this);
    quickStartFrame->setFrameStyle(QFrame::StyledPanel);
    auto *formLayout = new QFormLayout(quickStartFrame);
    formLayout->setContentsMargins(12, 10, 12, 10);
    formLayout->setSpacing(8);

    m_langCombo = new QComboBox(this);
    for (const auto &lang : flatlas::core::I18n::availableLanguages())
        m_langCombo->addItem(lang);
    m_langCombo->setCurrentText(flatlas::core::I18n::instance().currentLanguage());
    connect(m_langCombo, &QComboBox::currentTextChanged, this, [](const QString &lang) {
        flatlas::core::I18n::instance().setLanguage(lang);
        flatlas::core::Config::instance().setString(QStringLiteral("language"), lang);
        flatlas::core::Config::instance().save();
    });
    formLayout->addRow(tr("Language:"), m_langCombo);

    m_themeCombo = new QComboBox(this);
    for (const auto &theme : flatlas::core::Theme::instance().availableThemes())
        m_themeCombo->addItem(theme);
    m_themeCombo->setCurrentText(flatlas::core::Theme::instance().currentTheme());
    connect(m_themeCombo, &QComboBox::currentTextChanged, this, [](const QString &theme) {
        flatlas::core::Theme::instance().apply(theme);
        flatlas::core::Config::instance().setString(QStringLiteral("theme"), theme);
        flatlas::core::Config::instance().save();
    });
    formLayout->addRow(tr("Theme:"), m_themeCombo);

    m_updateCheck = new QCheckBox(tr("Automatically check for updates at startup"), this);
    m_updateCheck->setChecked(
        flatlas::core::Config::instance().getBool(QStringLiteral("autoUpdateCheck"), true));
    connect(m_updateCheck, &QCheckBox::toggled, this, [](bool checked) {
        flatlas::core::Config::instance().setBool(QStringLiteral("autoUpdateCheck"), checked);
        flatlas::core::Config::instance().save();
    });
    formLayout->addRow(tr("Update Check"), m_updateCheck);

    layout->addWidget(quickStartFrame);

    // --- IDS Toolchain Status ---
    m_idsStatusLabel = new QLabel(
        QStringLiteral("\u2714 ") +
        tr("IDS toolchain detected. IDS names and IDS info can be created and edited."), this);
    m_idsStatusLabel->setStyleSheet(QStringLiteral("QLabel { color: #44aa66; }"));
    layout->addWidget(m_idsStatusLabel);

    layout->addStretch();

    // --- Bottom buttons ---
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);

    auto *wikiBtn = new QPushButton(tr("Open Wiki"), this);
    wikiBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #1a6630; color: white; padding: 5px 14px;"
                        " border-radius: 3px; border: none; }"
                        "QPushButton:hover { background: #22883e; }"));
    connect(wikiBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/flathack/FLAtlas-V2/wiki")));
    });
    bottomLayout->addWidget(wikiBtn);

    auto *idsToolsBtn = new QPushButton(tr("Install IDS Tools"), this);
    connect(idsToolsBtn, &QPushButton::clicked, this, []() {
        // TODO: IDS tools installer
    });
    bottomLayout->addWidget(idsToolsBtn);

    bottomLayout->addStretch();

    auto *openMmBtn = new QPushButton(tr("Open Mod Manager"), this);
    openMmBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #2a5599; color: white; padding: 5px 14px;"
                        " border-radius: 3px; border: none; }"
                        "QPushButton:hover { background: #3566aa; }"));
    connect(openMmBtn, &QPushButton::clicked, this, [this]() {
        emit openModManagerRequested();
    });
    bottomLayout->addWidget(openMmBtn);

    layout->addLayout(bottomLayout);
}

} // namespace flatlas::ui
