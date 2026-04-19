// editors/ini/IniEditorPage.cpp – INI-Editor mit Syntax-Highlighting (Phase 6)

#include "IniEditorPage.h"
#include "IniCodeEditor.h"
#include "IniSyntaxHighlighter.h"
#include "infrastructure/parser/IniParser.h"
#include "infrastructure/parser/BiniDecoder.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMessageBox>

using namespace flatlas::infrastructure;

namespace flatlas::editors {

IniEditorPage::IniEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void IniEditorPage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupSearchBar();
    layout->addWidget(m_searchBar);
    m_searchBar->setVisible(false);

    m_editor = new IniCodeEditor(this);
    m_highlighter = new IniSyntaxHighlighter(m_editor->document());
    layout->addWidget(m_editor);

    connect(m_editor, &QPlainTextEdit::textChanged, this, &IniEditorPage::onTextChanged);

    // Toggle search bar with Ctrl+F
    auto *findAction = new QAction(this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() {
        m_searchBar->setVisible(!m_searchBar->isVisible());
        if (m_searchBar->isVisible()) {
            m_searchEdit->setFocus();
            m_searchEdit->selectAll();
        }
    });
    addAction(findAction);
}

void IniEditorPage::setupSearchBar()
{
    m_searchBar = new QToolBar(this);
    m_searchBar->setIconSize(QSize(16, 16));

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search..."));
    m_searchEdit->setMaximumWidth(250);
    m_searchBar->addWidget(m_searchEdit);

    auto *findNextAction = m_searchBar->addAction(tr("Next"));
    connect(findNextAction, &QAction::triggered, this, &IniEditorPage::findNext);

    auto *findPrevAction = m_searchBar->addAction(tr("Prev"));
    connect(findPrevAction, &QAction::triggered, this, &IniEditorPage::findPrev);

    m_searchBar->addSeparator();

    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("Replace..."));
    m_replaceEdit->setMaximumWidth(250);
    m_searchBar->addWidget(m_replaceEdit);

    auto *replaceAllAction = m_searchBar->addAction(tr("Replace All"));
    connect(replaceAllAction, &QAction::triggered, this, &IniEditorPage::replaceAll);

    m_searchBar->addSeparator();

    m_caseSensitiveCheck = new QCheckBox(tr("Case"), this);
    m_searchBar->addWidget(m_caseSensitiveCheck);

    m_statusLabel = new QLabel(this);
    m_searchBar->addWidget(m_statusLabel);

    // Enter in search field triggers findNext
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &IniEditorPage::findNext);
}

bool IniEditorPage::openFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray raw = file.readAll();
    file.close();

    QString text;
    if (BiniDecoder::isBini(raw)) {
        // BINI file: decode to INI text
        text = BiniDecoder::decode(raw);
        m_wasBini = true;
    } else {
        text = QString::fromUtf8(raw);
        m_wasBini = false;
    }

    m_filePath = filePath;
    m_editor->setPlainText(text);
    m_dirty = false;
    updateTitle();
    return true;
}

bool IniEditorPage::save()
{
    if (m_filePath.isEmpty())
        return false;
    return saveAs(m_filePath);
}

bool IniEditorPage::saveAs(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << m_editor->toPlainText();
    file.close();

    m_filePath = filePath;
    m_dirty = false;
    m_wasBini = false; // saved as text, no longer BINI
    updateTitle();
    return true;
}

QString IniEditorPage::filePath() const
{
    return m_filePath;
}

QString IniEditorPage::fileName() const
{
    return m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
}

bool IniEditorPage::isDirty() const
{
    return m_dirty;
}

void IniEditorPage::onTextChanged()
{
    if (!m_dirty) {
        m_dirty = true;
        updateTitle();
    }
}

void IniEditorPage::updateTitle()
{
    QString title = fileName();
    if (m_wasBini)
        title += tr(" [BINI→Text]");
    if (m_dirty)
        title += QStringLiteral(" *");
    emit titleChanged(title);
}

void IniEditorPage::findNext()
{
    const QString text = m_searchEdit->text();
    if (text.isEmpty())
        return;
    bool found = m_editor->findText(text, m_caseSensitiveCheck->isChecked(), false, true);
    m_statusLabel->setText(found ? QString() : tr("Not found"));
}

void IniEditorPage::findPrev()
{
    const QString text = m_searchEdit->text();
    if (text.isEmpty())
        return;
    bool found = m_editor->findText(text, m_caseSensitiveCheck->isChecked(), false, false);
    m_statusLabel->setText(found ? QString() : tr("Not found"));
}

void IniEditorPage::replaceAll()
{
    const QString search = m_searchEdit->text();
    const QString replace = m_replaceEdit->text();
    if (search.isEmpty())
        return;
    int count = m_editor->replaceAll(search, replace, m_caseSensitiveCheck->isChecked());
    m_statusLabel->setText(tr("%1 replaced").arg(count));
}

} // namespace flatlas::editors
