#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;

namespace flatlas::editors {

class IniFindReplaceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IniFindReplaceDialog(QWidget *parent = nullptr);

    QString findText() const;
    QString replaceText() const;
    QString globalSearchText() const;
    int requestedLineNumber() const;
    bool caseSensitive() const;
    bool useRegex() const;

    void setFindText(const QString &text);
    void focusFindField();

signals:
    void findNextRequested();
    void findPreviousRequested();
    void replaceAllRequested();
    void globalSearchRequested();
    void goToLineRequested(int lineNumber);

private:
    QLineEdit *m_findEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QLineEdit *m_globalSearchEdit = nullptr;
    QLineEdit *m_gotoLineEdit = nullptr;
    QCheckBox *m_caseSensitiveCheck = nullptr;
    QCheckBox *m_regexCheck = nullptr;
};

} // namespace flatlas::editors