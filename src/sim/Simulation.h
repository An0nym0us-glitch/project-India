#pragma once
#include "catalog/Catalog.h"
#include "comparison/ReferenceData.h"
#include "core/ModelParameters.h"
#include "core/Modifiers.h"
#include "engines/ConstructionEngine.h"
#include "engines/EconomyEngine.h"
#include "engines/EnvironmentEngine.h"
#include "engines/NetworkEngine.h"
#include "engines/PolicyEngine.h"
#include "engines/PopulationEngine.h"
#include "engines/UtilityEngine.h"
#include "gis/GeoDatabase.h"
#include "sim/WorldState.h"
#include <memory>

namespace isim {

// Immutable shared resources (loaded once).
struct SimulationResources {
    GeoDatabase geo;
    Catalog catalog;
    PolicyLibrary policies;
    ModelParameters params;
    ReferenceData reference;
    bool load(const QString &dataDir, QString *error = nullptr);
};

struct ScenarioSettings {
    QString name = QStringLiteral("New scenario");
    int startYear = 2025;
    double initialFundCr = 500000;
    quint64 seed = 42;
};

class Simulation {
public:
    explicit Simulation(const SimulationResources &res);
    Simulation(const Simulation &other) = default;

    void newScenario(const ScenarioSettings &s);
    // Install a loaded world (rebuilds derived state).
    void setWorld(const WorldState &w);
    WorldState &world() { return m_w; }
    const WorldState &world() const { return m_w; }
    ModelParameters &params() { return m_p; }
    const ModelParameters &params() const { return m_p; }
    void setParams(const ModelParameters &p) { m_p = p; }
    const SimulationResources &resources() const { return *m_res; }
    SimContext context();
    const NetworkModel &network() const { return m_net; }
    const EnvironmentGrid &environment() const { return m_env; }
    const UtilityEngine &utilities() const { return m_util; }
    const CpcbAqi &aqi() const { return m_envEngine.aqi(); }
    const Modifiers &modifiers() const { return m_mods; }

    void stepMonth();
    void runMonths(int n) { for (int i = 0; i < n; ++i) stepMonth(); }
    // Recompute all derived quantities without advancing time.
    void refresh();

    PlacementResult place(const PlacementRequest &req);
    bool demolish(int uid);
    void enactPolicy(const QString &id, double intensity);
    void repealPolicy(const QString &id);
    bool isPolicyActive(const QString &id) const;

    void setFastMode(bool f) { m_fast = f; }
    double developmentScore() const;

private:
    void aggregate();
    void recordHistory();
    void checkAlerts();
    void recordBaselines();

    const SimulationResources *m_res;
    WorldState m_w;
    ModelParameters m_p;
    Modifiers m_mods;
    NetworkModel m_net;
    EnvironmentGrid m_env;
    NetworkEngine m_netEngine;
    ConstructionEngine m_construction;
    UtilityEngine m_util;
    EconomyEngine m_economy;
    PopulationEngine m_population;
    SkillsEngine m_skills;
    TechnologyEngine m_tech;
    EnvironmentEngine m_envEngine;
    PolicyEngine m_policy;
    bool m_fast = false;
};

} // namespace isim
