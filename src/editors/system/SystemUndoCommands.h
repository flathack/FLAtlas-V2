#pragma once

#include <QUndoCommand>
#include <QVector3D>
#include <QVariant>
#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <memory>

#include "SystemPersistence.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"
#include "domain/ZoneItem.h"

namespace flatlas::editors {

class AddObjectCommand : public QUndoCommand {
public:
    explicit AddObjectCommand(domain::SystemDocument* doc,
                              std::shared_ptr<domain::SolarObject> obj,
                              const QString& text = QStringLiteral("Add Object"))
        : QUndoCommand(text), m_doc(doc), m_obj(std::move(obj)) {}

    void undo() override { if (m_doc) m_doc->removeObject(m_obj); }
    void redo() override { if (m_doc) m_doc->addObject(m_obj); }

private:
    QPointer<domain::SystemDocument> m_doc;
    std::shared_ptr<domain::SolarObject> m_obj;
};

class RemoveObjectCommand : public QUndoCommand {
public:
    explicit RemoveObjectCommand(domain::SystemDocument* doc,
                                 std::shared_ptr<domain::SolarObject> obj,
                                 const QString& text = QStringLiteral("Remove Object"))
        : QUndoCommand(text), m_doc(doc), m_obj(std::move(obj)) {}

    void undo() override { if (m_doc) m_doc->addObject(m_obj); }
    void redo() override { if (m_doc) m_doc->removeObject(m_obj); }

private:
    QPointer<domain::SystemDocument> m_doc;
    std::shared_ptr<domain::SolarObject> m_obj;
};

class MoveObjectCommand : public QUndoCommand {
public:
    explicit MoveObjectCommand(domain::SolarObject* obj,
                               const QVector3D& oldPos,
                               const QVector3D& newPos,
                               const QString& text = QStringLiteral("Move Object"))
        : QUndoCommand(text), m_obj(obj), m_oldPos(oldPos), m_newPos(newPos) {}

    void undo() override { if (m_obj) m_obj->setPosition(m_oldPos); }
    void redo() override { if (m_obj) m_obj->setPosition(m_newPos); }

    int id() const override { return 1; }

    bool mergeWith(const QUndoCommand* other) override {
        auto cmd = dynamic_cast<const MoveObjectCommand*>(other);
        if (!cmd || !m_obj || cmd->m_obj != m_obj)
            return false;
        m_newPos = cmd->m_newPos;
        return true;
    }

private:
    QPointer<domain::SolarObject> m_obj;
    QVector3D m_oldPos;
    QVector3D m_newPos;
};

class RotateObjectCommand : public QUndoCommand {
public:
    explicit RotateObjectCommand(domain::SolarObject* obj,
                                 const QVector3D& oldRotation,
                                 const QVector3D& newRotation,
                                 const QString& text = QStringLiteral("Rotate Object"))
        : QUndoCommand(text), m_obj(obj), m_oldRotation(oldRotation), m_newRotation(newRotation) {}

    void undo() override { if (m_obj) m_obj->setRotation(m_oldRotation); }
    void redo() override { if (m_obj) m_obj->setRotation(m_newRotation); }

private:
    QPointer<domain::SolarObject> m_obj;
    QVector3D m_oldRotation;
    QVector3D m_newRotation;
};

class ApplyObjectSectionCommand : public QUndoCommand {
public:
    explicit ApplyObjectSectionCommand(domain::SolarObject* obj,
                                       const infrastructure::IniSection& beforeSection,
                                       const infrastructure::IniSection& afterSection,
                                       const QString& text = QStringLiteral("Apply Object Changes"))
        : QUndoCommand(text), m_obj(obj), m_beforeSection(beforeSection), m_afterSection(afterSection) {}

    void undo() override {
        if (m_obj)
            SystemPersistence::applyObjectSection(*m_obj, m_beforeSection);
    }

    void redo() override {
        if (m_obj)
            SystemPersistence::applyObjectSection(*m_obj, m_afterSection);
    }

private:
    QPointer<domain::SolarObject> m_obj;
    infrastructure::IniSection m_beforeSection;
    infrastructure::IniSection m_afterSection;
};

class AddZoneCommand : public QUndoCommand {
public:
    explicit AddZoneCommand(domain::SystemDocument* doc,
                            std::shared_ptr<domain::ZoneItem> zone,
                            const QString& text = QStringLiteral("Add Zone"))
        : QUndoCommand(text), m_doc(doc), m_zone(std::move(zone)) {}

    void undo() override { if (m_doc) m_doc->removeZone(m_zone); }
    void redo() override { if (m_doc) m_doc->addZone(m_zone); }

private:
    QPointer<domain::SystemDocument> m_doc;
    std::shared_ptr<domain::ZoneItem> m_zone;
};

class MoveZoneCommand : public QUndoCommand {
public:
    explicit MoveZoneCommand(domain::ZoneItem* zone,
                             const QVector3D& oldPos,
                             const QVector3D& newPos,
                             const QString& text = QStringLiteral("Move Zone"))
        : QUndoCommand(text), m_zone(zone), m_oldPos(oldPos), m_newPos(newPos) {}

    void undo() override { if (m_zone) m_zone->setPosition(m_oldPos); }
    void redo() override { if (m_zone) m_zone->setPosition(m_newPos); }

    int id() const override { return 2; }

    bool mergeWith(const QUndoCommand* other) override {
        auto cmd = dynamic_cast<const MoveZoneCommand*>(other);
        if (!cmd || !m_zone || cmd->m_zone != m_zone)
            return false;
        m_newPos = cmd->m_newPos;
        return true;
    }

private:
    QPointer<domain::ZoneItem> m_zone;
    QVector3D m_oldPos;
    QVector3D m_newPos;
};

class RotateZoneCommand : public QUndoCommand {
public:
    explicit RotateZoneCommand(domain::ZoneItem* zone,
                               const QVector3D& oldRotation,
                               const QVector3D& newRotation,
                               const QString& text = QStringLiteral("Rotate Zone"))
        : QUndoCommand(text), m_zone(zone), m_oldRotation(oldRotation), m_newRotation(newRotation) {}

    void undo() override { if (m_zone) m_zone->setRotation(m_oldRotation); }
    void redo() override { if (m_zone) m_zone->setRotation(m_newRotation); }

private:
    QPointer<domain::ZoneItem> m_zone;
    QVector3D m_oldRotation;
    QVector3D m_newRotation;
};

class RemoveZoneCommand : public QUndoCommand {
public:
    explicit RemoveZoneCommand(domain::SystemDocument* doc,
                               std::shared_ptr<domain::ZoneItem> zone,
                               const QString& text = QStringLiteral("Remove Zone"))
        : QUndoCommand(text), m_doc(doc), m_zone(std::move(zone)) {}

    void undo() override { if (m_doc) m_doc->addZone(m_zone); }
    void redo() override { if (m_doc) m_doc->removeZone(m_zone); }

private:
    QPointer<domain::SystemDocument> m_doc;
    std::shared_ptr<domain::ZoneItem> m_zone;
};

class EditPropertyCommand : public QUndoCommand {
public:
    explicit EditPropertyCommand(QObject* target,
                                 const QByteArray& propertyName,
                                 const QVariant& oldValue,
                                 const QVariant& newValue,
                                 const QString& text = QStringLiteral("Edit Property"))
        : QUndoCommand(text), m_target(target), m_propertyName(propertyName),
          m_oldValue(oldValue), m_newValue(newValue) {}

    void undo() override { if (m_target) m_target->setProperty(m_propertyName.constData(), m_oldValue); }
    void redo() override { if (m_target) m_target->setProperty(m_propertyName.constData(), m_newValue); }

private:
    QPointer<QObject> m_target;
    QByteArray m_propertyName;
    QVariant m_oldValue;
    QVariant m_newValue;
};

} // namespace flatlas::editors
