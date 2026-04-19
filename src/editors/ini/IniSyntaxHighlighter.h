#pragma once
// editors/ini/IniSyntaxHighlighter.h
// TODO Phase 6
#include <QSyntaxHighlighter>
namespace flatlas::editors {
class IniSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit IniSyntaxHighlighter(QTextDocument *parent = nullptr);
protected:
    void highlightBlock(const QString &text) override;
};
} // namespace flatlas::editors
