#include <algorithm>
#include "AppController.h"
#include "ai/AppSettings.h"
#include "scenario/ScenarioStore.h"

namespace isim {

AppController::AppController(SimulationResources &res, QObject *parent)
    : QObject(parent), m_res(res), m_sim(std::make_unique<Simulation>(res)), m_ai(new AIManager(this))
{
    connect(&m_timer, &QTimer::timeout, this, [this]() { stepMonths(1); });
    setSpeed(m_speed);
}

void AppController::newScenario(const ScenarioSettings &s)
{
    pause();
    m_sim = std::make_unique<Simulation>(m_res);
    m_sim->newScenario(s);
    m_path.clear();
    m_dirty = false;
    m_sel = {};
    m_placing.clear();
    m_recsValid = false;
    emit worldReset();
    emit worldUpdated();
}

void AppController::installSimulation(const Simulation &s, const QString &path)
{
    pause();
    m_sim = std::make_unique<Simulation>(s);
    m_path = path;
    m_dirty = path.isEmpty();
    m_sel = {};
    m_recsValid = false;
    emit worldReset();
    emit worldUpdated();
}

bool AppController::loadScenario(const QString &path, QString *error)
{
    WorldState w;
    if (!ScenarioStore::load(path, w, error)) return false;
    pause();
    m_sim = std::make_unique<Simulation>(m_res);
    m_sim->setWorld(w);
    m_path = path;
    m_dirty = false;
    m_sel = {};
    m_recsValid = false;
    emit worldReset();
    emit worldUpdated();
    return true;
}

bool AppController::saveScenario(const QString &path, QString *error)
{
    if (!ScenarioStore::save(m_sim->world(), path, error)) return false;
    m_path = path;
    m_dirty = false;
    emit statusMessage(tr("Scenario saved: %1").arg(path));
    return true;
}

void AppController::play() { m_timer.start(); emit worldUpdated(); }
void AppController::pause() { m_timer.stop(); emit worldUpdated(); }
void AppController::setSpeed(double mps)
{
    m_speed = std::clamp(mps, 0.25, 24.0);
    m_timer.setInterval(int(1000.0 / m_speed));
}

void AppController::stepMonths(int n)
{
    for (int i = 0; i < n; ++i) m_sim->stepMonth();
    if (m_sim->world().month % 12 == 0) m_recsValid = false;
    changed(false);
}

PlacementResult AppController::place(const PlacementRequest &req)
{
    auto r = m_sim->place(req);
    if (r.ok) {
        AppSettings::pushRecent(req.defId);
        m_sim->refresh();
        m_recsValid = false;
        changed(true);
    }
    return r;
}

void AppController::demolish(int uid)
{
    if (m_sim->demolish(uid)) {
        if (m_sel.type == Selection::Facility && m_sel.index == uid) select({});
        m_recsValid = false;
        changed(true);
    }
}

void AppController::enactPolicy(const QString &id, double intensity) { m_sim->enactPolicy(id, intensity); m_sim->refresh(); m_recsValid = false; changed(false); }
void AppController::repealPolicy(const QString &id) { m_sim->repealPolicy(id); m_sim->refresh(); m_recsValid = false; changed(false); }

void AppController::setAdvisorDecision(const QString &recId, const QString &decision)
{
    if (decision.isEmpty()) m_sim->world().advisorDecisions.remove(recId);
    else m_sim->world().advisorDecisions.insert(recId, decision);
    for (auto &r : m_recs) if (r.id == recId) r.decision = decision;
    m_dirty = true;
    emit worldUpdated();
}

void AppController::select(const Selection &s)
{
    if (s == m_sel) return;
    m_sel = s;
    emit selectionChanged();
}

void AppController::beginPlacement(const QString &defId)
{
    m_placing = defId;
    select({Selection::Definition, -1, defId});
    emit placementChanged(defId);
}

void AppController::cancelPlacement()
{
    if (m_placing.isEmpty()) return;
    m_placing.clear();
    emit placementChanged({});
}

void AppController::focusDistrict(int di)
{
    if (di < 0 || di >= m_res.geo.districts().size()) return;
    emit focusRequested(m_res.geo.districts()[di].feature->bbox.adjusted(-0.6, -0.6, 0.6, 0.6));
}

const QVector<Recommendation> &AppController::recommendations()
{
    if (!m_recsValid) {
        auto ctx = m_sim->context();
        m_recs = Advisor::analyse(ctx, &m_res.reference);
        m_recsValid = true;
    }
    return m_recs;
}

void AppController::changed(bool structural)
{
    m_dirty = true;
    Q_UNUSED(structural);
    emit worldUpdated();
}

} // namespace isim
