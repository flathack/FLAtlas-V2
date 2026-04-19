#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace flatlas::core {

Theme &Theme::instance()
{
    static Theme theme;
    return theme;
}

QStringList Theme::availableThemes() const
{
    return {QStringLiteral("dark"), QStringLiteral("light"),
            QStringLiteral("founder"), QStringLiteral("xp")};
}

QString Theme::currentTheme() const { return m_current; }

// ── palette definitions ──────────────────────────────────────────────

const QHash<QString, Palette> &Theme::palettes()
{
    static const QHash<QString, Palette> p{
        {QStringLiteral("founder"), {
            {QStringLiteral("bg"),           QStringLiteral("#12122a")},
            {QStringLiteral("bg_alt"),       QStringLiteral("#0d0d25")},
            {QStringLiteral("bg_input"),     QStringLiteral("#0d0d22")},
            {QStringLiteral("bg_list"),      QStringLiteral("#0a0a1e")},
            {QStringLiteral("bg_toolbar"),   QStringLiteral("#0e0e28")},
            {QStringLiteral("bg_textedit"),  QStringLiteral("#08080f")},
            {QStringLiteral("fg"),           QStringLiteral("#ddddee")},
            {QStringLiteral("fg_dim"),       QStringLiteral("#444455")},
            {QStringLiteral("fg_accent"),    QStringLiteral("#99aaff")},
            {QStringLiteral("border"),       QStringLiteral("#333344")},
            {QStringLiteral("border_light"), QStringLiteral("#444466")},
            {QStringLiteral("btn_bg"),       QStringLiteral("#1e1e50")},
            {QStringLiteral("btn_hover"),    QStringLiteral("#2a2a70")},
            {QStringLiteral("sel_bg"),       QStringLiteral("#2a3070")},
            {QStringLiteral("list_hover"),   QStringLiteral("#1e2050")},
            {QStringLiteral("splitter"),     QStringLiteral("#222244")},
            {QStringLiteral("cb_checked"),   QStringLiteral("#5060c0")},
            {QStringLiteral("menu_bg"),      QStringLiteral("#16163a")},
            {QStringLiteral("scrollbar_bg"), QStringLiteral("#0a0a1e")},
            {QStringLiteral("scrollbar_fg"), QStringLiteral("#333344")},
        }},
        {QStringLiteral("dark"), {
            {QStringLiteral("bg"),           QStringLiteral("#171a1f")},
            {QStringLiteral("bg_alt"),       QStringLiteral("#1d2229")},
            {QStringLiteral("bg_input"),     QStringLiteral("#11151b")},
            {QStringLiteral("bg_list"),      QStringLiteral("#11151b")},
            {QStringLiteral("bg_toolbar"),   QStringLiteral("#1b2027")},
            {QStringLiteral("bg_textedit"),  QStringLiteral("#0f1318")},
            {QStringLiteral("fg"),           QStringLiteral("#e4e9f0")},
            {QStringLiteral("fg_dim"),       QStringLiteral("#7e8897")},
            {QStringLiteral("fg_accent"),    QStringLiteral("#7dc4ff")},
            {QStringLiteral("border"),       QStringLiteral("#2c333d")},
            {QStringLiteral("border_light"), QStringLiteral("#3a4451")},
            {QStringLiteral("btn_bg"),       QStringLiteral("#232a33")},
            {QStringLiteral("btn_hover"),    QStringLiteral("#2d3743")},
            {QStringLiteral("sel_bg"),       QStringLiteral("#255d90")},
            {QStringLiteral("list_hover"),   QStringLiteral("#242b35")},
            {QStringLiteral("splitter"),     QStringLiteral("#2a313b")},
            {QStringLiteral("cb_checked"),   QStringLiteral("#4ea3ea")},
            {QStringLiteral("menu_bg"),      QStringLiteral("#1b2027")},
            {QStringLiteral("scrollbar_bg"), QStringLiteral("#12161c")},
            {QStringLiteral("scrollbar_fg"), QStringLiteral("#3a4552")},
        }},
        {QStringLiteral("light"), {
            {QStringLiteral("bg"),           QStringLiteral("#f6f8fb")},
            {QStringLiteral("bg_alt"),       QStringLiteral("#edf1f6")},
            {QStringLiteral("bg_input"),     QStringLiteral("#ffffff")},
            {QStringLiteral("bg_list"),      QStringLiteral("#ffffff")},
            {QStringLiteral("bg_toolbar"),   QStringLiteral("#e9edf3")},
            {QStringLiteral("bg_textedit"),  QStringLiteral("#ffffff")},
            {QStringLiteral("fg"),           QStringLiteral("#1f2937")},
            {QStringLiteral("fg_dim"),       QStringLiteral("#6b7280")},
            {QStringLiteral("fg_accent"),    QStringLiteral("#0d4f94")},
            {QStringLiteral("border"),       QStringLiteral("#cfd7e3")},
            {QStringLiteral("border_light"), QStringLiteral("#b9c5d6")},
            {QStringLiteral("btn_bg"),       QStringLiteral("#f3f6fb")},
            {QStringLiteral("btn_hover"),    QStringLiteral("#e8edf5")},
            {QStringLiteral("sel_bg"),       QStringLiteral("#2f7dd1")},
            {QStringLiteral("list_hover"),   QStringLiteral("#eef3fa")},
            {QStringLiteral("splitter"),     QStringLiteral("#d4dbe6")},
            {QStringLiteral("cb_checked"),   QStringLiteral("#2f7dd1")},
            {QStringLiteral("menu_bg"),      QStringLiteral("#f4f7fc")},
            {QStringLiteral("scrollbar_bg"), QStringLiteral("#edf2f8")},
            {QStringLiteral("scrollbar_fg"), QStringLiteral("#c4cedd")},
        }},
        {QStringLiteral("xp"), {
            {QStringLiteral("bg"),           QStringLiteral("#dbeafc")},
            {QStringLiteral("bg_alt"),       QStringLiteral("#eaf2fd")},
            {QStringLiteral("bg_input"),     QStringLiteral("#ffffff")},
            {QStringLiteral("bg_list"),      QStringLiteral("#ffffff")},
            {QStringLiteral("bg_toolbar"),   QStringLiteral("#c9ddf7")},
            {QStringLiteral("bg_textedit"),  QStringLiteral("#ffffff")},
            {QStringLiteral("fg"),           QStringLiteral("#001a52")},
            {QStringLiteral("fg_dim"),       QStringLiteral("#4b628a")},
            {QStringLiteral("fg_accent"),    QStringLiteral("#003c9d")},
            {QStringLiteral("border"),       QStringLiteral("#7f9db9")},
            {QStringLiteral("border_light"), QStringLiteral("#96b3d5")},
            {QStringLiteral("btn_bg"),       QStringLiteral("#e7f0fc")},
            {QStringLiteral("btn_hover"),    QStringLiteral("#d7e9ff")},
            {QStringLiteral("sel_bg"),       QStringLiteral("#316ac5")},
            {QStringLiteral("list_hover"),   QStringLiteral("#edf5ff")},
            {QStringLiteral("splitter"),     QStringLiteral("#9fbbe0")},
            {QStringLiteral("cb_checked"),   QStringLiteral("#316ac5")},
            {QStringLiteral("menu_bg"),      QStringLiteral("#f2f7ff")},
            {QStringLiteral("scrollbar_bg"), QStringLiteral("#d6e6fb")},
            {QStringLiteral("scrollbar_fg"), QStringLiteral("#9fb7d8")},
            {QStringLiteral("variant"),      QStringLiteral("xp")},
        }},
    };
    return p;
}

// ── stylesheet generation ────────────────────────────────────────────

QString Theme::generateStylesheet(const Palette &p)
{
    QString ss = QStringLiteral(
        "QWidget { color:%1; font-family:\"Segoe UI\",Tahoma,\"Microsoft Sans Serif\",Arial,sans-serif; }\n"
        "QMainWindow, QDialog, QWidget#centralWidget { background:%2; }\n"
        "QFrame, QStackedWidget, QDockWidget { background:%2; }\n"
        "QGroupBox { border:1px solid %3; margin-top:10px; padding:5px;"
            " border-radius:4px; background:%4; }\n"
        "QGroupBox::title { color:%5; }\n"
        "QPushButton { background:%6; border:1px solid %7;"
            " padding:4px 8px; border-radius:3px; color:%1; }\n"
        "QPushButton:hover { background:%8; }\n"
        "QPushButton:disabled { color:%9; }\n"
        "QTextEdit, QPlainTextEdit { background:%10; color:%1;"
            " border:1px solid %3; selection-background-color:%11; }\n"
        "QLineEdit { background:%12; border:1px solid %7;"
            " padding:3px; border-radius:2px; color:%1; selection-background-color:%11; }\n"
        "QComboBox, QSpinBox, QDoubleSpinBox { background:%12; color:%1;"
            " border:1px solid %7; padding:3px; border-radius:2px; }\n"
        "QComboBox QAbstractItemView, QMenu, QListView {"
            " background:%13; color:%1; border:1px solid %7;"
            " selection-background-color:%11; }\n"
        "QListWidget, QTreeWidget, QTableWidget {"
            " background:%14; color:%1; border:1px solid %3;"
            " alternate-background-color:%4; gridline-color:%3;"
            " selection-background-color:%11; }\n"
        "QListWidget::item:hover, QTreeWidget::item:hover, QTableWidget::item:hover { background:%15; }\n"
        "QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected { background:%11; color:#ffffff; }\n"
        "QHeaderView::section { background:%16; color:%1; border:1px solid %3; padding:4px; }\n"
        "QTabWidget::pane { border:1px solid %3; background:%2; }\n"
        "QTabBar::tab { background:%6; color:%1; border:1px solid %7;"
            " padding:5px 10px; margin-right:2px; }\n"
        "QTabBar::tab:selected { background:%2; border-bottom-color:%2; }\n"
        "QTabBar::tab:hover { background:%8; }\n"
        "QToolBar { background:%16; border-bottom:1px solid %3;"
            " spacing:4px; padding:2px; }\n"
        "QStatusBar { background:%16; color:%5; }\n"
        "QStatusBar QLabel { color:%1; }\n"
        "QSplitter::handle { background:%17; width:3px; }\n"
        "QCheckBox { color:%1; spacing:5px; }\n"
        "QCheckBox::indicator { width:14px; height:14px;"
            " border:1px solid %7; border-radius:2px; background:%6; }\n"
        "QCheckBox::indicator:checked { background:%18; }\n"
        "QScrollBar:vertical { background:%19; width:10px; border:none; }\n"
        "QScrollBar::handle:vertical { background:%20; border-radius:4px; }\n"
        "QMenu { background:%13; color:%1; border:1px solid %7; }\n"
        "QMenu::item { padding:6px 24px; }\n"
        "QMenu::item:selected { background:%8; }\n"
        "QToolTip { background:%13; color:%1; border:1px solid %7; padding:4px; }\n"
        "QDockWidget::title { background:%16; padding:4px; }\n"
    ).arg(
        p[QStringLiteral("fg")],          // %1
        p[QStringLiteral("bg")],          // %2
        p[QStringLiteral("border")],      // %3
        p[QStringLiteral("bg_alt")],      // %4
        p[QStringLiteral("fg_accent")],   // %5
        p[QStringLiteral("btn_bg")],      // %6
        p[QStringLiteral("border_light")],// %7
        p[QStringLiteral("btn_hover")],   // %8
        p[QStringLiteral("fg_dim")]       // %9
    ).arg(
        p[QStringLiteral("bg_textedit")], // %10
        p[QStringLiteral("sel_bg")],      // %11
        p[QStringLiteral("bg_input")],    // %12
        p[QStringLiteral("menu_bg")],     // %13
        p[QStringLiteral("bg_list")],     // %14
        p[QStringLiteral("list_hover")],  // %15
        p[QStringLiteral("bg_toolbar")],  // %16
        p[QStringLiteral("splitter")],    // %17
        p[QStringLiteral("cb_checked")],  // %18
        p[QStringLiteral("scrollbar_bg")]  // %19
    ).arg(
        p[QStringLiteral("scrollbar_fg")] // %20
    );

    // XP Luna gradient overrides
    if (p.value(QStringLiteral("variant")) == QStringLiteral("xp")) {
        ss += QStringLiteral(
            "QMenuBar {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f7fbff,stop:1 #d6e7fb);"
            " border-bottom:1px solid #7f9db9; }\n"
            "QToolBar {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #edf5ff,stop:1 #c6dbf7);"
            " border-bottom:1px solid #7f9db9; }\n"
            "QPushButton {"
            " border:1px solid #6f8fb5; border-radius:2px; padding:4px 10px;"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ffffff,stop:1 #d9e9fb);"
            " color:#001a52; }\n"
            "QPushButton:hover {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ffffff,stop:1 #cfe3fb); }\n"
            "QPushButton:pressed {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #c4dbf8,stop:1 #eaf2fd); }\n"
            "QTabBar::tab {"
            " border:1px solid #7f9db9; border-bottom:none;"
            " border-top-left-radius:3px; border-top-right-radius:3px;"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f8fcff,stop:1 #dceafd);"
            " color:#00225f; }\n"
            "QTabBar::tab:selected { background:#ffffff; color:#003c9d; }\n"
            "QHeaderView::section {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f8fcff,stop:1 #d7e8fd);"
            " color:#002a72; border:1px solid #9ab3d4; }\n"
        );
    }

    return ss;
}

// ── QPalette setup ───────────────────────────────────────────────────

void Theme::applyQPalette(const Palette &p)
{
    QPalette pal;
    pal.setColor(QPalette::Window,          QColor(p[QStringLiteral("bg")]));
    pal.setColor(QPalette::Base,            QColor(p[QStringLiteral("bg_input")]));
    pal.setColor(QPalette::AlternateBase,   QColor(p[QStringLiteral("bg_alt")]));
    pal.setColor(QPalette::ToolTipBase,     QColor(p[QStringLiteral("menu_bg")]));
    pal.setColor(QPalette::ToolTipText,     QColor(p[QStringLiteral("fg")]));
    pal.setColor(QPalette::Text,            QColor(p[QStringLiteral("fg")]));
    pal.setColor(QPalette::WindowText,      QColor(p[QStringLiteral("fg")]));
    pal.setColor(QPalette::Button,          QColor(p[QStringLiteral("btn_bg")]));
    pal.setColor(QPalette::ButtonText,      QColor(p[QStringLiteral("fg")]));
    pal.setColor(QPalette::Highlight,       QColor(p[QStringLiteral("sel_bg")]));
    pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    pal.setColor(QPalette::PlaceholderText, QColor(p[QStringLiteral("fg_dim")]));
    qApp->setPalette(pal);
}

// ── apply ────────────────────────────────────────────────────────────

void Theme::apply(const QString &themeName)
{
    const auto &allPalettes = palettes();
    const Palette &pal = allPalettes.contains(themeName)
        ? allPalettes[themeName]
        : allPalettes[QStringLiteral("founder")];

    m_current = themeName;
    applyQPalette(pal);
    qApp->setStyleSheet(generateStylesheet(pal));
    emit themeChanged(themeName);
}

} // namespace flatlas::core
