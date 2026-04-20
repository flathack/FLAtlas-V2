#pragma once

#include <QUndoCommand>
#include <QVector3D>
#include <QVariant>
#include <QByteArray>
#include <QObject>
#include <memory>

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

    void undo() override { m_doc->removeObject(m_obj); }
    void redo() override { m_doc->addObject(m_obj); }

private:
    domain::SystemDocument* m_doc;
    std::shared_ptr<domain::SolarObject> m_obj;
};

class RemoveObjectCommand : public QUndoCommand {
public:
    explicit RemoveObjectCommand(domain::SystemDocument* doc,
                                 std::shared_ptr<domain::SolarObject> obj,
                                 const QString& text = QStringLiteral("Remove Object"))
        : QUndoCommand(text), m_doc(doc), m_obj(std::move(obj)) {}

    void undo() override { m_doc->addObject(m_obj); }
    void redo() override { m_doc->removeObject(m_obj); }

private:
    domain::SystemDocument* m_doc;
    std::shared_ptr<domain::SolarObject> m_obj;
};

class MoveObjectCommand : public QUndoCommand {
public:
    explicit MoveObjectCommand(domain::SolarObject* obj,
                               const QVector3D& oldPos,
                               const QVector3D& newPos,
                               const QString& text = QStringLiteral("Move Object"))
        : QUndoCommand(text), m_obj(obj), m_oldPos(oldPos), m_newPos(newPos) {}

    void undo() override { m_obj->setPosition(m_oldPos); }
    void redo() override { m_obj->setPosition(m_newPos); }

    int id() const override { return 1; }

    bool mergeWith(const QUndoCommand* other) override {
        auto cmd = dynamic_cast<const MoveObjectCommand*>(other);
        if (!cmd || cmd->m_obj != m_obj)
            return false;
        m_newPos = cmd->m_newPos;
        return true;
    }

private:
    domain::SolarObject* m_obj;
    QVector3D m_oldPos;
    QVector3D m_newPos;
};

class AddZoneCommand : public QUndoCommand {
public:
    explicit AddZoneCommand(domain::SystemDocument* doc,
                            std::shared_ptr<domain::ZoneItem> zone,
                            const QString& text = QStringLiteral("Add Zone"))
        : QUndoCommand(text), m_doc(doc), m_zone(std::move(zone)) {}

    void undo() override { m_doc->removeZone(m_zone); }
    void redo() override { m_doc->addZone(m_zone); }

private:
    domain::SystemDocument* m_doc;
    std::shared_ptr<domain::ZoneItem> m_zone;
};

class MoveZoneCommand : public QUndoCommand {
public:
    explicit MoveZoneCommand(domain::ZoneItem* zone,
                             const QVector3D& oldPos,
                             const QVector3D& newPos,
                             const QString& text = QStringLiteral("Move Zone"))
        : QUndoCommand(text), m_zone(zone), m_oldPos(oldPos), m_newPos(newPos) {}

    void undo() override { m_zone->setPosition(m_oldPos); }
    void redo() override { m_zone->setPosition(m_newPos); }

    int id() const override { return 2; }

    bool mergeWith(const QUndoCommand* other) override {
        auto cmd = dynamic_cast<const MoveZoneCommand*>(other);
        if (!cmd || cmd->m_zone != m_zone)
            return false;
        m_newPos = cmd->m_newPos;
        return true;
    }

private:
    domain::ZoneItem* m_zone;
    QVector3D m_oldPos;
    QVector3D m_newPos;
};

class RemoveZoneCommand : public QUndoCommand {
public:
    explicit RemoveZoneCommand(domain::SystemDocument* doc,
                               std::shared_ptr<domain::ZoneItem> zone,
                               const QString& text = QStringLiteral("Remove Zone"))
        : QUndoCommand(text), m_doc(doc), m_zone(std::move(zone)) {}

    void undo() override { m_doc->addZone(m_zone); }
    void redo() override { m_doc->removeZone(m_zone); }

private:
    domain::SystemDocument* m_doc;
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

    void undo() override { m_target->setProperty(m_propertyName.constData(), m_oldValue); }
    void redo() override { m_target->setProperty(m_propertyName.constData(), m_newValue); }

private:
    QObject* m_target;
    QByteArray m_propertyName;
    QVariant m_oldValue;
    QVariant m_newValue;
};

} // namespace flatlas::editors
