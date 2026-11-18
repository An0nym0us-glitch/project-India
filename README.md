# India National Development & Policy Simulator

A GIS-based national planning, infrastructure, economic, technology, environmental and policy
simulation for India, built as a native **Qt 6 / C++17** desktop application.

You act as the government of India. The map shows real physical geography and real population,
but the **development layer starts empty**: no roads, power grid, cities, factories or institutions.
You build them, set policies, and watch the consequences. Everything is computed by the C++
simulation engine; an optional AI assistant (local Qwen3-1.7B via llama.cpp, or OpenAI / Gemini)
only *explains* results.

> This is a planning laboratory, not a forecast. Every number is labelled as **measured**,
> **estimated**, **modelling assumption**, **user-configured** or **simulated**, and projections
> are shown with uncertainty bands.

## Download (Windows .exe)

Every push builds a Windows package on GitHub Actions:

1. Open the repository's **Actions** tab, then the latest **Build** run.
2. Download the artifact **IndiaDevelopmentSimulator-windows-x64** (a zip).
3. Unzip anywhere and run `IndiaDevelopmentSimulator.exe`.

The zip contains the executable, the Qt runtime, the `data/` folder and a llama.cpp server
(`llama/` CPU build, `llama-vulkan/` GPU build) for the optional local AI.
Tagged versions (`v*`) are also published as GitHub Releases.

## First launch

A setup wizard asks for:

* **Local AI model (optional)**: pick a `.gguf` file already on your PC
  (recommended: `Qwen3-1.7B-Q4_K_M.gguf`, ≈1.1 GB, from Hugging Face). The app validates the file
  and can test-load it. **Nothing is downloaded automatically.**
* **OpenAI / Gemini API keys (optional)**: stored in Windows Credential Manager, never in plain
  settings. They can be added later in *Tools → Settings — AI providers*.
* **Default AI** (local / OpenAI / Gemini / none) and your first scenario.

The simulator works fully without any AI.

## Build from source

Requirements: CMake ≥ 3.21, a C++17 compiler, Qt ≥ 6.4 (Core, Gui, Widgets, Network, Sql, Charts, Concurrent, Test).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

```bash
ctest --test-dir build --output-on-failure
```

```bash
./build/src/IndiaDevelopmentSimulator
```

On Windows use the Qt MSVC kit (e.g. Qt 6.5 `msvc2019_64` with the Qt Charts module) and run
`windeployqt` on the executable; the CI workflow `.github/workflows/build.yml` shows the exact steps.

## What is in the box

| Area | Implemented |
|---|---|
| Map & GIS | 36 states/UTs, 640 districts (Census 2011), rivers, lakes, coastline, physical regions, neighbours; pan/zoom, selection, search, 10 data layers, PM2.5 grid |
| Infrastructure | 152 data-driven facility types in 11 categories, 58 goods; search by name, industry, technology, category, function or required resource; favourites, recent, recommendations |
| Placement | Point and line placement with live suitability assessment, snapping, river-crossing/bridge logic, construction over time with public/private financing |
| Networks | Road/rail/waterway/metro connectivity, multimodal nodes, port/air/fibre access, power-grid islands with transmission, piped-water networks |
| Simulation | Electricity balance with seasonal renewables, storage and reliability; water, sewage, waste; cohort population model; migration & housing; workforce skills; supply chains & imports; GDP, employment, budget & debt |
| Technology | 14-domain maturity tree; facilities produce only when skills, power, inputs, transport, technology maturity and supporting industries exist |
| Policy | 34 policies (tax, industry, R&D, education, health, agriculture, migration & population, energy, environment, trade, labour, housing, infrastructure, governance) with lags, ramps, uncertain effectiveness and documented side effects; coercive policies carry modelled harms |
| Advisor | National Development Advisor with 18 analyses; each recommendation has rationale, locations, supporting infrastructure, cost, benefits, drawbacks, opportunity cost, short/long-term effects, risks, alternatives and evidence; accept/reject/save |
| Environment | Gridded emissions → dispersion (seasonal winds, mixing height, washout, secondary PM) → CPCB AQI per district |
| Comparison | Simulated India vs real India, China, USA, Japan and Russia (World Bank WDI, fixed baseline year, unavailable data marked) with tables, rankings, gaps and trend charts |
| Scenarios | Versioned SQLite scenario files, save/load/duplicate, monthly/yearly stepping and continuous run, 1–50 year ensemble projections with 10–90% bands, strategy presets A–D, scenario comparison |
| AI | Provider abstraction: local llama.cpp server (GGUF), OpenAI-compatible, Gemini; validation of structured answers and flagging of any number not present in simulation data |

See [`docs/STATUS.md`](docs/STATUS.md) for what is complete and what remains, [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
[`docs/DATA_SOURCES.md`](docs/DATA_SOURCES.md) and [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md).
