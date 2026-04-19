#pragma once
// tools/HelpBrowser.h – In-App-Hilfe-System
// TODO Phase 23
#include <QDialog>
namespace flatlas::tools {
class HelpBrowser : public QDialog {
    Q_OBJECT
public:
    explicit HelpBrowser(QWidget *parent = nullptr);
    void showTopic(const QString &topicId);
};
} // namespace flatlas::tools
