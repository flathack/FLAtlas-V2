#pragma once
// rendering/flight/FlightController.h – Flugsteuerung
// TODO Phase 15
#include <QObject>
namespace flatlas::rendering {
class FlightController : public QObject {
    Q_OBJECT
public:
    enum State { Docked, Normal, Cruise, Formation };
    Q_ENUM(State)
    explicit FlightController(QObject *parent = nullptr) : QObject(parent) {}
    State state() const { return m_state; }
private:
    State m_state = Docked;
};
} // namespace flatlas::rendering
