# Implementation status

Verified on Linux (GCC 13, Qt 6.4): the application builds, all unit/integration tests pass
(`ctest`: core, simulation, AI), and the main window, dashboard and comparison views render
(offscreen screenshots). The Windows package is built and smoke-tested by GitHub Actions.

## Milestones

| # | Milestone | Status |
|---|---|---|
| 1 | Qt application, interface, India map, state boundaries, selection, zoom/pan, empty development layer | Done |
| 2 | District boundaries, map layers, search, facility catalogue, information panels | Done |
| 3 | Construction system, resource requirements, placement, roads & connectivity | Done |
| 4 | Electricity, water, transport simulation; population & housing; economy | Done (simplified models, documented) |
| 5 | Technology industries, semiconductor & AI infrastructure, prerequisites & progression | Done |
| 6 | Government policies, National Development Advisor, suitability system | Done |
| 7 | Environmental/AQI simulation, global country comparison | Done |
| 8 | Qwen3-1.7B integration, AI analysis interface, scenario management, long-term projections | Done; local inference verified only with a mock server here (no model in CI) |

## Known limitations / remaining work

* **Model calibration**: most facility costs and many coefficients are indicative assumptions
  (labelled as such). They are not calibrated against Indian national accounts; projections are
  illustrative.
* **Air quality** is a simplified box/advection model with uniform seasonal winds, not a chemical
  transport model; ozone is indicative only; AQI uses monthly means as a proxy for 24-hour averages.
* **Economy**: no prices, inflation, exchange-rate or interest-rate dynamics; services trade not modelled.
* **Population** age structure is national; districts share the same natural growth rate.
* **Terrain**: no DEM; mineral deposits at state level only; only major rivers.
* **GIS**: 2011 district set; boundaries simplified for display.
* **Local AI** has been exercised against an OpenAI-compatible mock server in tests; running a real
  GGUF model requires the user's model file and the bundled llama-server.
* **Not yet implemented**: undo/redo of placements, editing model parameters from the UI (view only),
  per-facility custom names in the UI, PDF/CSV report export, translations.
