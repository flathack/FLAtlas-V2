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

    /// Human-readable display name resolved from universe.ini +
    /// Freelancer IDS strings (e.g. "New York" for system "Li01").
    /// Empty if the lookup is unavailable. Purely informational — the
    /// serialiser never writes this back to the system .ini.
    QString displayName() const;
    void setDisplayName(const QString &name);

    QString filePath() const;
    void setFilePath(const QString &path);

    double navMapScale() const;
    void setNavMapScale(double scale);

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
    QString m_displayName;
    QString m_filePath;
    // Freelancer's NavMapScale default: 1.36. Vanilla system .ini files that
    // omit the value rely on the universe.ini NavMapScale; if that lookup
    // fails for any reason, the correct fallback is still the Freelancer
    // default, not 1.0 (which would produce a NavMap grid that is too large
    // and push objects visually toward the edge).
    double m_navMapScale = 1.36;
    bool m_dirty = false;
    QVector<std::shared_ptr<SolarObject>> m_objects;
    QVector<std::shared_ptr<ZoneItem>> m_zones;
};

} // namespace flatlas::domain
