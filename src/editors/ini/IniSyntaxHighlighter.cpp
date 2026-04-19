#include "IniSyntaxHighlighter.h"
namespace flatlas::editors {
IniSyntaxHighlighter::IniSyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {}
void IniSyntaxHighlighter::highlightBlock(const QString &) {} // TODO Phase 6
} // namespace flatlas::editors
