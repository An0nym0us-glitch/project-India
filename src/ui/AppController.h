#pragma once
#include "advisor/Advisor.h"
#include "ai/AIManager.h"
#include "sim/Simulation.h"
#include <QObject>
#include <QTimer>
#include <memory>

namespace isim {

struct Selection {
    enum Type { None, State, District, Facility, Definition } type = None;
    int index = -1;       // state / district index, facility uid
    QString defId;        // catalogue definition
    bool operator==(const Selection &o) const { return type == o.type && index == o.index && defId == o.defId; }
};

// Owns the simulation and coordinates the UI (single source of truth).
class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(SimulationResources &res, QObject *parent = nullptr);
    SimulationResources &resources() { return m_res; }
    Simulation &sim() { return *m_sim; }
    AIManager *ai() { return m_ai; }

    void newScenario(const ScenarioSettings &s);
    bool loadScenario(const QString &path, QString *error);
    bool saveScenario(const QString &path, QString *error);
    void installSimulation(const Simulation &s, const QString &path = {});
    QString scenarioPath() const { return m_path; }
    bool isDirty() const { return m_dirty; }

    // time control
    bool isRunning() const { return m_timer.isActive(); }
    void play();
    void pause();
    void setSpeed(double monthsPerSecond);
    double speed() const { return m_speed; }
    void stepMonths(int n);

    // editing
    PlacementResult place(const PlacementRequest &req);
    void demolish(int uid);
    void enactPolicy(const QString &id, double intensity);
    void repealPolicy(const QString &id);
    void setAdvisorDecision(const QString &recId, const QString &decision);

    // selection & placement mode
    const Selection &selection() const { return m_sel; }
    void select(const Selection &s);
    void beginPlacement(const QString &defId);
    void cancelPlacement();
    QString placementDef() const { return m_placing; }
    void focusDistrict(int district);

    const QVector<Recommendation> &recommendations();
    void invalidateRecommendations() { m_recsValid = false; }

signals:
    void worldReset();
    void worldUpdated();          // after any change (month step, placement, policy)
    void selectionChanged();
    void placementChanged(const QString &defId);
    void focusRequested(const QRectF &lonlatBounds);
    void statusMessage(const QString &text);

private:
    void changed(bool structural);
    SimulationResources &m_res;
    std::unique_ptr<Simulation> m_sim;
    AIManager *m_ai;
    QTimer m_timer;
    double m_speed = 2.0;
    QString m_path;
    bool m_dirty = false;
    Selection m_sel;
    QString m_placing;
    QVector<Recommendation> m_recs;
    bool m_recsValid = false;
};

} // namespace isim
