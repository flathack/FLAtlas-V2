#pragma once
// infrastructure/updater/AutoUpdater.h – ZIP-basiertes Auto-Update-System
// TODO Phase 22
#include <QObject>
#include <QString>
namespace flatlas::infrastructure {
class AutoUpdater : public QObject {
    Q_OBJECT
public:
    explicit AutoUpdater(QObject *parent = nullptr) : QObject(parent) {}
    void checkForUpdates();
signals:
    void updateAvailable(const QString &version, const QString &downloadUrl);
    void noUpdateAvailable();
    void checkFailed(const QString &error);
};
} // namespace flatlas::infrastructure
