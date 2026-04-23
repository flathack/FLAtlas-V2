#include "IniFindReplaceDialog.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace flatlas::editors {

IniFindReplaceDialog::IniFindReplaceDialog(QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint)
{
    setModal(false);
    setWindowTitle(tr("Find / Replace"));
    setObjectName(QStringLiteral("iniFindReplaceDialog"));
    resize(520, 220);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    grid->addWidget(new QLabel(tr("Find"), this), 0, 0);
    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText(tr("Text or regex"));
    grid->addWidget(m_findEdit, 0, 1, 1, 3);

    grid->addWidget(new QLabel(tr("Replace"), this), 1, 0);
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("Replacement"));
    grid->addWidget(m_replaceEdit, 1, 1, 1, 3);

    m_caseSensitiveCheck = new QCheckBox(tr("Case sensitive"), this);
    grid->addWidget(m_caseSensitiveCheck, 2, 1);
    m_regexCheck = new QCheckBox(tr("Regex"), this);
    grid->addWidget(m_regexCheck, 2, 2);

    auto *findButtons = new QHBoxLayout();
    auto *findNextButton = new QPushButton(tr("Find Next"), this);
    connect(findNextButton, &QPushButton::clicked, this, &IniFindReplaceDialog::findNextRequested);
    findButtons->addWidget(findNextButton);
    auto *findPrevButton = new QPushButton(tr("Find Previous"), this);
    connect(findPrevButton, &QPushButton::clicked, this, &IniFindReplaceDialog::findPreviousRequested);
    findButtons->addWidget(findPrevButton);
    auto *replaceAllButton = new QPushButton(tr("Replace All"), this);
    connect(replaceAllButton, &QPushButton::clicked, this, &IniFindReplaceDialog::replaceAllRequested);
    findButtons->addWidget(replaceAllButton);
    findButtons->addStretch();
    rootLayout->addLayout(grid);
    rootLayout->addLayout(findButtons);

    auto *globalRow = new QHBoxLayout();
    globalRow->addWidget(new QLabel(tr("Search Files"), this));
    m_globalSearchEdit = new QLineEdit(this);
    m_globalSearchEdit->setPlaceholderText(tr("Search across the current workspace"));
    globalRow->addWidget(m_globalSearchEdit, 1);
    auto *globalButton = new QPushButton(tr("Search"), this);
    connect(globalButton, &QPushButton::clicked, this, &IniFindReplaceDialog::globalSearchRequested);
    globalRow->addWidget(globalButton);
    rootLayout->addLayout(globalRow);

    auto *gotoRow = new QHBoxLayout();
    gotoRow->addWidget(new QLabel(tr("Go to line"), this));
    m_gotoLineEdit = new QLineEdit(this);
    m_gotoLineEdit->setPlaceholderText(tr("Line number"));
    m_gotoLineEdit->setMaximumWidth(120);
    gotoRow->addWidget(m_gotoLineEdit);
    auto *gotoButton = new QPushButton(tr("Go"), this);
    connect(gotoButton, &QPushButton::clicked, this, [this]() {
        emit goToLineRequested(requestedLineNumber());
    });
    gotoRow->addWidget(gotoButton);
    gotoRow->addStretch();
    rootLayout->addLayout(gotoRow);

    connect(m_findEdit, &QLineEdit::returnPressed, this, &IniFindReplaceDialog::findNextRequested);
    connect(m_globalSearchEdit, &QLineEdit::returnPressed, this, &IniFindReplaceDialog::globalSearchRequested);
    connect(m_gotoLineEdit, &QLineEdit::returnPressed, this, [this]() {
        emit goToLineRequested(requestedLineNumber());
    });
}

QString IniFindReplaceDialog::findText() const
{
    return m_findEdit ? m_findEdit->text() : QString();
}

QString IniFindReplaceDialog::replaceText() const
{
    return m_replaceEdit ? m_replaceEdit->text() : QString();
}

QString IniFindReplaceDialog::globalSearchText() const
{
    return m_globalSearchEdit ? m_globalSearchEdit->text() : QString();
}

int IniFindReplaceDialog::requestedLineNumber() const
{
    return m_gotoLineEdit ? m_gotoLineEdit->text().toInt() : 0;
}

bool IniFindReplaceDialog::caseSensitive() const
{
    return m_caseSensitiveCheck && m_caseSensitiveCheck->isChecked();
}

bool IniFindReplaceDialog::useRegex() const
{
    return m_regexCheck && m_regexCheck->isChecked();
}

void IniFindReplaceDialog::setFindText(const QString &text)
{
    if (!m_findEdit)
        return;
    m_findEdit->setText(text);
    m_findEdit->selectAll();
}

void IniFindReplaceDialog::focusFindField()
{
    if (!m_findEdit)
        return;
    show();
    raise();
    activateWindow();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

} // namespace flatlas::editors