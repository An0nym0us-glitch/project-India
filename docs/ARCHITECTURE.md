# Architecture

```
src/
  core/        Provenance, ModelParameters (value + range + source), Modifiers, DataPaths
  gis/         GeoJSON loader, GeoDatabase (states, districts, physical layers, spatial queries, search)
  catalog/     FacilityDefinition, Catalog (facilities, goods, tech tree, search)
  sim/         WorldState (serialisable state), Simulation (orchestrator)
  engines/     ConstructionEngine, NetworkEngine (transport / grid / water topology), UtilityEngine,
               EconomyEngine (workforce, efficiency, supply chains, GDP, budget), PopulationEngine,
               SkillsEngine, TechnologyEngine, PolicyEngine, EnvironmentEngine + CPCB AQI
  advisor/     SuitabilityEngine (pre-placement assessment), Advisor (National Development Advisor)
  comparison/  ReferenceData (World Bank snapshot), ComparisonEngine, strategy presets
  scenario/    ScenarioStore (versioned SQLite), ProjectionRunner (ensembles on a thread pool)
  ai/          AIProvider abstraction, LocalLlamaProvider, OpenAI/Gemini providers, SecretStore,
               AIContextBuilder, AIResponseValidator, AIManager, AppSettings
  ui/          Qt Widgets: MainWindow, MapView, panels, dashboard, wizard, settings
  app/         main.cpp
tests/         QtTest unit & integration tests
data/          Bundled data and data-driven definitions
tools/         Data pipeline scripts
```

`isim_core` (everything except `ui/`) is a static library with no widget dependency, so the
simulation runs headless in tests and in the projection ensemble.

## Monthly step order (`Simulation::stepMonth`)

1. Policy modifiers (lag, ramp-in, sampled/central effectiveness, governance-trust scaling)
2. Construction progress and capital payments (public treasury; private capital pool constraint)
3. Network rebuild when facilities change (river crossings & bridges, transport components,
   access flags, power islands, water networks)
4. Workforce allocation and facility pre-efficiency (staffing, lagged input availability,
   transport, technology maturity, supporting industries)
5. Utilities: electricity per grid island (seasonal output, storage firming, reliability,
   distribution limits), water, sewage, waste → facility power/water factors
6. Final efficiency, services, food, goods balance & trade, labour market, GDP, budget
7. Skills (graduates, inflows, attrition, brain drain), technology research and level-ups
8. Population (cohort survival, fertility, ageing, migration, urbanisation, housing)
9. Environment (gridded emissions → advection/deposition/secondary PM → district AQI)
10. Aggregates, history, alerts

## Design rules

* **Data-driven**: facilities, goods, technology levels, policies and coefficients are JSON; new
  facility types need no engine changes.
* **Dependencies, not independent buildings**: a facility's output is the product of its staffing,
  electricity, water, inputs, transport access, technology maturity and supporting industry.
* **Provenance everywhere**: parameters carry value, range, unit, status and source; the UI shows them.
* **AI isolation**: language models receive structured JSON produced by the engine; answers are
  schema-validated and any number not in the context is flagged as unverified.
* **Versioned saves**: `.indiasim` files are SQLite databases with `schema_version` in `meta`.

## Performance

A monthly step takes ≈30–60 ms (Release, 640 districts, 128×134 air-quality grid). Projections
run ensemble members in parallel; fast mode solves air quality quarterly with fewer iterations.
