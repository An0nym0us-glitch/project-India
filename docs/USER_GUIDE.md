# User guide

## Layout
* **Top bar**: date, treasury (and debt), GDP, population, electricity served, development score, time controls.
* **Left – Planning**: *Build* (catalogue and search), *Advisor*, *Policies*, *Technology*.
* **Centre – Map**: scroll to zoom, drag to pan, click a district (zoomed in) or state; Alt+click selects the state.
  *Data layer* colours districts (population density, electricity, water, unemployment, housing shortage,
  GDP per capita, AQI, urban share, 2011 literacy). *Layers* toggles geography, networks and the PM2.5 grid.
  The map search box finds states, districts, rivers and regions.
* **Right – Details**: full information sheet for the selected facility type, placed facility, district or state;
  *AI analysis* tab.
* **Bottom – Timeline**: notifications, construction progress, economic events, environmental alerts.

## Building
1. Search the catalogue (Ctrl+F). Choose the field (name, industry, technology, category, function, resource).
2. Select an item to read its 20-point information sheet.
3. *Place on map*. While placing, the cursor colour shows suitability (green recommended, amber gaps,
   orange not recommended, red not feasible) and the Information panel shows the full assessment and
   recommended prerequisite investments with reasons.
4. Lines (roads, railways, power lines, pipelines, fibre): click points, double-click or Enter to finish,
   Backspace to undo, Esc to cancel. Ends snap to existing lines and network nodes.
5. Shift+click keeps placing the same facility. Right-click a facility to demolish it.

Key rules: electricity reaches consumers only through substations/distribution networks connected to
generation (same district or via transmission lines); road and rail lines crossing mapped rivers need a
bridge; factories need workers with the right skills, inputs, transport access, technology maturity and
supporting industries before they produce.

## Time
Space runs/pauses; *+1 month* / *+1 year* step manually; choose the speed in the toolbar.

## Advisor
Recommendations are recomputed from the simulation yearly or on *Re-analyse*. *Accept & place* centres the
map on the suggested district and starts placement; *Save* and *Reject* keep a record in the scenario.

## Policies
Select a policy to see cost, revenue impact, benefits, unintended consequences, short/long-term effects,
affected groups, state differences, challenges and uncertainty. Set the intensity and *Enact*.

## Dashboard
*National statistics* (all indicators + history charts), *Global country comparison* (choose the baseline
year; hover cells for notes), *Scenarios & projections* (tick scenarios/strategies, choose horizon and
ensemble size, *Run projections*), *Data sources & assumptions* (every coefficient with its status).

## Scenarios
File → New / Open / Save / Save as / Duplicate. Files use the `.indiasim` extension.

## AI assistant
Choose the provider in the AI panel. For the local model, *Load model* starts llama-server with your GGUF
file (status shows Loading → Ready). Ask for a national report interpretation, alternatives, or explanations
from the Information, Advisor and Policy panels. Numbers not present in the simulation data are highlighted
as *unverified*. For GPU acceleration on NVIDIA cards, select `llama-vulkan/llama-server.exe` in
Settings → AI Providers → Local model and keep *GPU layers* at 99.
