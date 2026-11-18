# Data sources, processing and what must be downloaded

## Bundled with the application (in `data/`)

All bundled data is small (≈5 MB) and redistributable; see `LICENSE-DATA.md`.

| File | Content | Processing |
|---|---|---|
| `geo/states.geojson` | 36 states/UTs | DataMeet `States/Admin2.shp`, simplified 0.006°, area computed |
| `geo/districts.geojson` | 640 districts with Census 2011 attributes | DataMeet `Districts/Census_2011`, joined on census code (640/640 matched), assigned to current states (incl. Telangana, Ladakh) by spatial join, simplified 0.008° |
| `geo/rivers.geojson`, `lakes`, `coastline`, `physical_regions`, `neighbours`, `india_outline` | Physical geography | Natural Earth, clipped to the region |
| `reference/worldbank_reference.json` | 48 indicators × 5 countries, 1990–2024, plus India's age structure | World Bank WDI bulk download; missing values kept as missing |
| `catalog/*.json` | 152 facilities, 58 goods | Authored with `tools/gen_catalog.py` |
| `tech/tech_tree.json`, `policies/policies.json`, `model_parameters.json`, `aqi/cpcb_breakpoints.json` | Model definitions | Hand-authored, documented |

Rebuild the geodata (needs `pyshp`, `shapely` and the raw downloads listed in the script header):

```bash
python3 tools/build_geodata.py <raw_dir> data
```

Refresh the World Bank snapshot:

```bash
python3 tools/fetch_worldbank.py
```

## Not bundled (user downloads, optional)

* **Language model** for the local assistant: a GGUF file such as Qwen3-1.7B Q4_K_M (≈1.1 GB) from
  https://huggingface.co/Qwen/Qwen3-1.7B-GGUF. Chosen by the user in the setup wizard; never downloaded automatically.

## Known data limitations

* District boundaries are the 2011 census set (640 districts); newer districts are not split out.
  District attributes are 2011 values; the simulation scales populations to the World Bank national
  total for the reference year using 2011 district shares (a stated simplification).
* Natural Earth rivers include only major rivers, so smaller rivers (e.g. the Hasdeo) are absent;
  river-proximity checks and bridge requirements apply to mapped rivers only.
* No elevation model is bundled: "mountainous" terrain is approximated by Natural Earth range polygons
  plus the Himalayan/north-eastern hill states.
* Mineral deposit locations are not bundled; mining facilities are restricted to states with known
  occurrences (Indian Bureau of Mines, state level).
* World Bank rail-network length (`IS.RRD.TOTL.KM`) is no longer published and is shown as unavailable.
  Several indicators (R&D, researchers, physicians, beds) lag by 2–4 years; the comparison labels the
  actual year used.
