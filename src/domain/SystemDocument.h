#pragma once
// domain/SystemDocument.h – Systemdokument-Datenmodell
// TODO Phase 2

#include <QObject>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <memory>

namespace flatlas::domain {

class SolarObject;
class ZoneItem;

/// Repräsentiert ein geladenes Freelancer-System mit Objekten und Zonen.
class SystemDocument : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)

public:
    explicit SystemDocument(QObject *parent = nullptr);
    ~SystemDocument() override;

    QString name() const;
    void setName(const QString &name);

    QString filePath() const;
    void setFilePath(const QString &path);

    bool isDirty() const;
    void setDirty(bool dirty);

    const QVector<std::shared_ptr<SolarObject>> &objects() const;
    const QVector<std::shared_ptr<ZoneItem>> &zones() const;

    void addObject(std::shared_ptr<SolarObject> obj);
    void removeObject(const std::shared_ptr<SolarObject> &obj);

    void addZone(std::shared_ptr<ZoneItem> zone);
    void removeZone(const std::shared_ptr<ZoneItem> &zone);

signals:
    void nameChanged();
    void dirtyChanged();
    void objectAdded(const std::shared_ptr<SolarObject> &obj);
    void objectRemoved(const std::shared_ptr<SolarObject> &obj);
    void zoneAdded(const std::shared_ptr<ZoneItem> &zone);
    void zoneRemoved(const std::shared_ptr<ZoneItem> &zone);

private:
    QString m_name;
    QString m_filePath;
    bool m_dirty = false;
    QVector<std::shared_ptr<SolarObject>> m_objects;
    QVector<std::shared_ptr<ZoneItem>> m_zones;
};

} // namespace flatlas::domain
