#!/usr/bin/env python3
"""Build the simplified GeoJSON layers bundled with the application.

Inputs (download first, see docs/DATA_SOURCES.md):
  DataMeet  States/Admin2.shp            (CC BY 4.0)  - state / UT boundaries
  DataMeet  Districts/Census_2011/2011_Dist.shp (CC BY 2.5 IN) - district boundaries
  Natural Earth 10m rivers, lakes, geography regions; 50m admin-0 (public domain)
  Census of India 2011 district primary census abstract (CSV compilation)
Usage:  python3 tools/build_geodata.py <raw_dir> <out_data_dir>
Requires: pyshp, shapely
"""
import csv, json, math, sys, os
import shapefile
from shapely.geometry import shape, mapping, box
from shapely.ops import unary_union

raw, out = sys.argv[1], sys.argv[2]
BBOX = box(60.0, 2.0, 102.0, 40.0)

def rnd(geom, nd=4):
    def r(c):
        if isinstance(c, (list, tuple)) and c and isinstance(c[0], (int, float)):
            return [round(c[0], nd), round(c[1], nd)]
        return [r(x) for x in c]
    m = mapping(geom)
    return {"type": m["type"], "coordinates": r(m["coordinates"])}

def area_km2(geom):
    # Equal-area approximation: scale lon by cos(lat) of the centroid.
    lat = geom.centroid.y
    return geom.area * (111.32 ** 2) * math.cos(math.radians(lat))

def write(name, feats):
    path = os.path.join(out, "geo", name)
    with open(path, "w") as f:
        json.dump({"type": "FeatureCollection", "features": feats}, f, separators=(",", ":"))
    print(name, len(feats), os.path.getsize(path) // 1024, "KiB")

def fix(g):
    return g if g.is_valid else g.buffer(0)

# --- States ---------------------------------------------------------------
sr = shapefile.Reader(os.path.join(raw, "Admin2"))
states = []
for i, sh in enumerate(sr.shapes()):
    name = sr.record(i)[0].strip()
    g = fix(shape(sh.__geo_interface__))
    states.append((name, g))
CODE = {}
for name, _ in states:
    CODE[name] = "".join(w[0] for w in name.replace("&", " ").split() if w[0].isalpha()).upper()[:4]
CODE.update({"Andhra Pradesh": "AP", "Arunachal Pradesh": "AR", "Assam": "AS", "Bihar": "BR", "Chhattisgarh": "CG",
    "Goa": "GA", "Gujarat": "GJ", "Haryana": "HR", "Himachal Pradesh": "HP", "Jharkhand": "JH", "Karnataka": "KA",
    "Kerala": "KL", "Madhya Pradesh": "MP", "Maharashtra": "MH", "Manipur": "MN", "Meghalaya": "ML", "Mizoram": "MZ",
    "Nagaland": "NL", "Odisha": "OD", "Punjab": "PB", "Rajasthan": "RJ", "Sikkim": "SK", "Tamil Nadu": "TN",
    "Telangana": "TS", "Tripura": "TR", "Uttar Pradesh": "UP", "Uttarakhand": "UK", "West Bengal": "WB",
    "Andaman & Nicobar": "AN", "Chandigarh": "CH", "Dadra and Nagar Haveli and Daman and Diu": "DH", "Delhi": "DL",
    "Jammu & Kashmir": "JK", "Ladakh": "LA", "Lakshadweep": "LD", "Puducherry": "PY"})
UT = {"Andaman & Nicobar", "Chandigarh", "Dadra and Nagar Haveli and Daman and Diu", "Delhi", "Jammu & Kashmir",
      "Ladakh", "Lakshadweep", "Puducherry"}

# --- Census 2011 attributes -----------------------------------------------
census = {}
with open(os.path.join(raw, "census2011_districts.csv")) as f:
    for row in csv.DictReader(f):
        census[int(row["District code"])] = row
KEEP = ["Population", "Male", "Female", "Literate", "Workers", "Main_Workers", "Marginal_Workers",
        "Cultivator_Workers", "Agricultural_Workers", "Household_Workers", "Other_Workers", "Households",
        "Rural_Households", "Urban_Households", "Housholds_with_Electric_Lighting", "Households_with_Internet",
        "Graduate_Education", "Higher_Education", "Secondary_Education", "Age_Group_0_29", "Age_Group_30_49",
        "Age_Group_50", "Having_latrine_facility_within_the_premises_Total_Households",
        "Main_source_of_drinking_water_Tapwater_Households", "Condition_of_occupied_census_houses_Dilapidated_Households"]

# --- Districts --------------------------------------------------------------
dr = shapefile.Reader(os.path.join(raw, "2011_Dist"))
dfeats = []
state_pop = {n: 0 for n, _ in states}
missing = 0
for i, sh in enumerate(dr.shapes()):
    rec = dr.record(i)
    code = int(rec[4])
    if code == 0 or code not in census:
        missing += 1
        continue
    g = fix(shape(sh.__geo_interface__))
    pt = g.representative_point()
    st = next((n for n, sg in states if sg.contains(pt)), None)
    if st is None:  # fall back to largest overlap
        st = max(states, key=lambda s: s[1].intersection(g).area)[0]
    c = census[code]
    props = {"id": f"D{code:03d}", "name": rec[0].strip(), "state": st, "state2011": rec[1].strip(),
             "censusCode": code, "areaKm2": round(area_km2(g), 1)}
    cen = {}
    for k in KEEP:
        try:
            cen[k] = int(float(c.get(k, "0") or 0))
        except ValueError:
            cen[k] = 0
    props["census2011"] = cen
    state_pop[st] += cen["Population"]
    s = g.simplify(0.008, preserve_topology=True)
    dfeats.append({"type": "Feature", "properties": props, "geometry": rnd(s)})
print("districts skipped (no census code):", missing)
write("districts.geojson", dfeats)

sfeats = []
for name, g in states:
    s = g.simplify(0.006, preserve_topology=True)
    sfeats.append({"type": "Feature", "properties": {"id": CODE[name], "name": name,
        "type": "Union Territory" if name in UT else "State", "areaKm2": round(area_km2(g), 1),
        "census2011Population": state_pop[name]}, "geometry": rnd(s)})
write("states.geojson", sfeats)

india = unary_union([g for _, g in states]).buffer(0)
india_buf = india.buffer(0.6)

# --- Physical layers (Natural Earth) ------------------------------------------
def ne(path, keep, filt=None, clip=BBOX, tol=0.01):
    r = shapefile.Reader(path)
    flds = [f[0] for f in r.fields[1:]]
    feats = []
    for i, sh in enumerate(r.shapes()):
        rec = {k.lower(): v for k, v in zip(flds, r.record(i))}
        if filt and not filt(rec):
            continue
        g = shape(sh.__geo_interface__)
        if not g.is_valid:
            g = g.buffer(0)
        g = g.intersection(clip)
        if g.is_empty:
            continue
        g = g.simplify(tol, preserve_topology=True)
        feats.append({"type": "Feature", "properties": {k: rec.get(k) for k in keep}, "geometry": rnd(g)})
    return feats

write("rivers.geojson", ne(os.path.join(raw, "ne_10m_rivers_lake_centerlines/ne_10m_rivers_lake_centerlines"),
      ["name", "scalerank", "featurecla"], clip=india_buf))
write("lakes.geojson", ne(os.path.join(raw, "ne_10m_lakes/ne_10m_lakes"), ["name", "scalerank"], clip=india_buf))
write("physical_regions.geojson", ne(os.path.join(raw, "ne_10m_geography_regions_polys/ne_10m_geography_regions_polys"),
      ["name", "featurecla", "scalerank"], filt=lambda r: r.get("featurecla") in ("Range/mtn", "Plateau", "Desert", "Plain", "Delta", "Basin", "Lowland", "Valley", "Peninsula", "Coast", "Island group", "Wetlands"),
      clip=india_buf, tol=0.02))
write("neighbours.geojson", ne(os.path.join(raw, "ne_50m_admin_0_countries/ne_50m_admin_0_countries"),
      ["name", "iso_a3"], filt=lambda r: r.get("iso_a3") != "IND", clip=box(40.0, -15.0, 125.0, 55.0), tol=0.02))
# India outline (used for coastline distance & land mask)
write("india_outline.geojson", [{"type": "Feature", "properties": {"name": "India"},
      "geometry": rnd(india.simplify(0.01, preserve_topology=True))}])
write("coastline.geojson", ne(os.path.join(raw, "ne_10m_coastline/ne_10m_coastline"), ["featurecla"], clip=india_buf, tol=0.01))
