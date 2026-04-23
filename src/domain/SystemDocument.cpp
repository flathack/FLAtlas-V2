#include "SystemDocument.h"
#include "SolarObject.h"
#include "ZoneItem.h"

namespace flatlas::domain {

SystemDocument::SystemDocument(QObject *parent) : QObject(parent) {}
SystemDocument::~SystemDocument() = default;

QString SystemDocument::name() const { return m_name; }
void SystemDocument::setName(const QString &name) { if (m_name != name) { m_name = name; emit nameChanged(); } }

QString SystemDocument::filePath() const { return m_filePath; }
void SystemDocument::setFilePath(const QString &path) { m_filePath = path; }

double SystemDocument::navMapScale() const { return m_navMapScale; }
void SystemDocument::setNavMapScale(double scale) { m_navMapScale = scale > 0.0 ? scale : 1.36; }

bool SystemDocument::isDirty() const { return m_dirty; }
void SystemDocument::setDirty(bool dirty) { if (m_dirty != dirty) { m_dirty = dirty; emit dirtyChanged(); } }

const QVector<std::shared_ptr<SolarObject>> &SystemDocument::objects() const { return m_objects; }
const QVector<std::shared_ptr<ZoneItem>> &SystemDocument::zones() const { return m_zones; }

void SystemDocument::addObject(std::shared_ptr<SolarObject> obj)
{
    m_objects.append(obj);
    emit objectAdded(obj);
    setDirty(true);
}

void SystemDocument::removeObject(const std::shared_ptr<SolarObject> &obj)
{
    m_objects.removeOne(obj);
    emit objectRemoved(obj);
    setDirty(true);
}

void SystemDocument::addZone(std::shared_ptr<ZoneItem> zone)
{
    m_zones.append(zone);
    emit zoneAdded(zone);
    setDirty(true);
}

void SystemDocument::removeZone(const std::shared_ptr<ZoneItem> &zone)
{
    m_zones.removeOne(zone);
    emit zoneRemoved(zone);
    setDirty(true);
}

} // namespace flatlas::domain
