#!/usr/bin/env python3
"""Authoring tool for the facility catalogue (data/catalog/facilities_*.json) and goods.

The JSON files are what the application loads; this script keeps them consistent.
All costs/capacities are INDICATIVE MODELLING ASSUMPTIONS (order of magnitude,
based on publicly reported Indian project costs where noted) unless the
provenance says otherwise. Edit here, then run:  python3 tools/gen_catalog.py
"""
import json, os, collections
OUT = os.path.join(os.path.dirname(__file__), "..", "data", "catalog")
FAC = collections.OrderedDict()

def F(id, name, cat, sub, industry, desc, purpose, capex, months, land=0, opex=None, power=0, water=0,
      cjobs=0, jobs=None, out=None, inputs=None, req=None, em=None, tags=(), tech=(), risks=(), env="",
      econ="", function="", geometry="point", network="", nodeOf=(), public=True, basis="", conf="low", source=""):
    assert id not in FAC, id
    FAC[id] = dict(id=id, name=name, category=cat, subcategory=sub, industry=industry, function=function or sub,
        description=desc, purpose=purpose, capexCr=capex, constructionMonths=months, landHa=land,
        opexCrPerYear=opex if opex is not None else round(capex * 0.03, 2), powerDemandMW=power, waterDemandMLD=water,
        constructionJobs=cjobs, operationsJobs=jobs or {}, outputs=out or {}, inputs=[{"good": g, "qtyPerYear": q} for g, q in (inputs or {}).items()],
        requires=req or {}, emissions=em or {}, tags=list(tags), technologies=list(tech), risks=list(risks),
        environmentNote=env, economicNote=econ, geometry=geometry, network=network, nodeOf=list(nodeOf), publicOwned=public,
        provenance={"kind": "assumption", "confidence": conf, "basis": basis or "Indicative order-of-magnitude modelling assumption", "source": source})

def J(u=0, s=0, e=0, r=0, m=0):
    return {k: v for k, v in dict(unskilled=u, skilled=s, engineer=e, scientist=r, medical=m).items() if v}

COAL_STATES = ["Jharkhand", "Odisha", "Chhattisgarh", "West Bengal", "Madhya Pradesh", "Telangana", "Maharashtra", "Uttar Pradesh"]
IBM = "Indian Bureau of Mines / Ministry of Coal (state-level occurrence, indicative)"
WIND_STATES = ["Gujarat", "Tamil Nadu", "Karnataka", "Maharashtra", "Rajasthan", "Andhra Pradesh", "Madhya Pradesh", "Telangana"]
NIWE = "NIWE wind potential assessment: high-potential states (indicative)"
SOLAR_STATES = ["Rajasthan", "Gujarat", "Karnataka", "Andhra Pradesh", "Telangana", "Madhya Pradesh", "Maharashtra", "Tamil Nadu", "Uttar Pradesh", "Punjab", "Haryana", "Ladakh", "Odisha", "Chhattisgarh", "Bihar", "Jharkhand", "Delhi"]

# ============================ RESIDENTIAL & URBAN ============================
R = "Residential & Urban"
F("village_cluster", "Village development cluster (500 homes)", R, "Rural housing", "Construction",
  "Clustered rural housing with basic internal roads, drainage and community spaces for ~500 households.",
  "Improves rural housing quality and capacity; prerequisite for rural service delivery.", 15, 12, land=20, cjobs=300,
  out={"housingPersons": 2300}, tags=["housing", "rural_housing"], power=0.3, water=0.15,
  risks=["Low density raises per-capita cost of utilities", "Needs road access for services"],
  basis="PMAY-G unit assistance ≈₹1.2-1.3 lakh plus beneficiary & infrastructure share (≈₹3 lakh/home)")
F("residential_building", "Residential building (100 units)", R, "Housing", "Construction",
  "Mid-rise apartment building with 100 dwelling units.", "Adds urban housing in small increments.", 35, 18, land=0.5, cjobs=150,
  out={"housingPersons": 450}, power=0.4, water=0.06, tags=["housing", "urban_housing"], req={"urban": True, "transport": ["road"]},
  basis="≈₹35 lakh per unit construction cost incl. common areas")
F("affordable_housing", "Affordable housing project (2,000 units)", R, "Housing", "Construction",
  "EWS/LIG housing project of 2,000 compact units with shared amenities.", "Reduces urban housing shortage for low-income households.", 220, 24, land=10, cjobs=1200,
  out={"housingPersons": 9000}, power=4, water=1.2, tags=["housing", "urban_housing", "affordable"], req={"transport": ["road"]},
  risks=["Peripheral locations can increase commute times", "Quality and maintenance risks"],
  basis="≈₹11 lakh per unit (PMAY-U AHP-type projects)")
F("apartment_complex", "Apartment complex (1,000 units)", R, "Housing", "Construction",
  "Mid-income apartment complex.", "Urban housing capacity.", 420, 30, land=4, cjobs=800,
  out={"housingPersons": 4500}, power=3, water=0.6, tags=["housing", "urban_housing"], req={"transport": ["road"], "urban": True},
  basis="≈₹42 lakh per unit")
F("highrise_towers", "High-rise residential towers (3,000 units)", R, "Housing", "Construction",
  "High-density tower cluster; efficient land use but heavy utility loads.", "Dense urban housing.", 1800, 42, land=6, cjobs=2500,
  out={"housingPersons": 13500}, power=12, water=1.9, tags=["housing", "urban_housing", "high_density"], req={"transport": ["road"], "urban": True},
  risks=["High water and sewage load concentrated in one location", "Traffic congestion without transit"], basis="≈₹60 lakh per unit")
F("luxury_apartments", "Luxury apartments (500 units)", R, "Housing", "Construction",
  "Premium residential development.", "High-income housing; limited effect on shortage.", 1500, 36, land=5, cjobs=900,
  out={"housingPersons": 2000}, power=5, water=0.5, tags=["housing", "urban_housing", "luxury"], req={"transport": ["road"], "urban": True},
  risks=["Little impact on low-income housing shortage", "High per-capita resource use"], basis="≈₹3 crore per unit")
F("housing_complex", "Gated housing complex (1,500 units)", R, "Housing", "Construction",
  "Gated complex with internal utilities.", "Urban/peri-urban housing.", 750, 30, land=12, cjobs=1100,
  out={"housingPersons": 6700}, power=5, water=0.9, tags=["housing", "urban_housing"], req={"transport": ["road"]}, basis="≈₹50 lakh per unit")
F("township", "Integrated township (20,000 units)", R, "Townships", "Construction",
  "Planned township with housing, local roads, schools, clinics and commercial space.", "Large-scale planned urbanisation.", 10000, 60, land=400, cjobs=12000,
  jobs=J(u=3000, s=1500, e=200), out={"housingPersons": 90000, "schoolSeats": 8000, "valueAddedCrPerYear": 400}, power=60, water=13, tags=["housing", "urban_housing", "township"],
  req={"transport": ["road"]}, risks=["Requires trunk water, power and transit connections", "Risk of under-occupancy if jobs are not nearby"], basis="Indicative: ₹50 lakh per unit incl. infrastructure")
F("smart_city", "Smart city development", R, "Townships", "Urban development",
  "Greenfield planned city with digital utilities, transit-oriented layout, and integrated services.", "Creates a new urban growth pole.", 50000, 96, land=5000, cjobs=40000,
  jobs=J(u=20000, s=15000, e=3000, r=200), out={"housingPersons": 500000, "schoolSeats": 50000, "hospitalBeds": 500, "valueAddedCrPerYear": 6000},
  power=500, water=75, tags=["housing", "urban_housing", "smart_city", "urban_centre"], tech=["urban systems", "IoT"],
  req={"transport": ["road", "rail"], "supporting": [{"tag": "power_plant", "count": 1, "radiusKm": 300, "why": "A city of 500,000 needs ~500 MW of firm supply"}]},
  risks=["Very high capital cost", "Historically slow occupancy of greenfield cities", "Land acquisition disputes"], basis="Indicative; comparable to large greenfield city projects (e.g. GIFT, Dholera phases)")
F("urban_infrastructure", "Urban infrastructure package", R, "Urban infrastructure", "Urban development",
  "Storm-water drainage, street upgrades, footpaths, street lighting and utility ducts for an urban district.", "Raises liveability and reduces flood losses.", 1000, 24, land=0, cjobs=3000,
  out={"valueAddedCrPerYear": 50}, power=2, tags=["urban_infrastructure"], req={"urban": True}, basis="Indicative AMRUT/Smart City Mission package scale")
C = "Commercial & Industrial Zones"
F("commercial_building", "Commercial building", C, "Commercial", "Services",
  "Multi-storey commercial building for shops and offices.", "Services employment.", 100, 18, land=1, power=2, water=0.05, cjobs=200,
  jobs=J(u=500, s=900, e=50), out={"valueAddedCrPerYear": 120}, tags=["commercial", "services"], public=False, req={"transport": ["road"], "urban": True}, basis="Indicative")
F("office_complex", "Office complex / IT park", C, "Commercial", "IT services",
  "Grade-A office campus suited to IT/ITES and global capability centres.", "High-value services employment and exports.", 1500, 30, land=10, power=25, water=0.8, cjobs=1500,
  jobs=J(u=1500, s=4000, e=14000), out={"valueAddedCrPerYear": 4200}, tags=["commercial", "services", "it_services"], public=False, tech=["IT services"],
  req={"transport": ["road"], "urban": True, "supporting": [{"tag": "fibre_node", "count": 1, "radiusKm": 50, "why": "IT services need high-capacity fibre connectivity"},
       {"tag": "engineering_college", "count": 1, "radiusKm": 500, "why": "A local pipeline of engineering graduates sustains IT hiring"}]},
  basis="IT services revenue ≈₹30 lakh per employee, ~70% value added")
F("shopping_centre", "Shopping centre", C, "Commercial", "Retail",
  "Organised retail mall.", "Retail services employment.", 500, 24, land=5, power=8, water=0.2, cjobs=600, jobs=J(u=2000, s=1000),
  out={"valueAddedCrPerYear": 250}, tags=["commercial", "services", "retail"], public=False, req={"transport": ["road"], "urban": True})
F("warehouse", "Warehouse", C, "Logistics", "Logistics",
  "Modern Grade-A warehouse (≈50,000 m²).", "Storage for manufacturing and distribution.", 100, 12, land=8, power=1, cjobs=150, jobs=J(u=250, s=60),
  out={"valueAddedCrPerYear": 30}, tags=["logistics", "warehouse"], public=False, req={"transport": ["road"]})
F("industrial_park", "Industrial park", C, "Industrial zones", "Industrial infrastructure",
  "Serviced industrial land with internal roads, power, water and effluent treatment.", "Lowers set-up cost and time for factories nearby (supporting infrastructure).", 2000, 36, land=800, power=5, water=2, cjobs=3000,
  jobs=J(u=200, s=150, e=30), out={"valueAddedCrPerYear": 60}, tags=["industrial_park", "industrial_infrastructure"], req={"transport": ["road"]},
  risks=["Land acquisition and displacement", "Under-utilisation if anchor investors don't arrive"], basis="Indicative; NICDC industrial node infrastructure costs")
F("sez", "Special economic zone", C, "Industrial zones", "Industrial infrastructure",
  "Export-oriented zone with customs facilities, single-window clearances and serviced plots.", "Attracts export manufacturing and services.", 5000, 48, land=2000, power=20, water=5, cjobs=6000,
  jobs=J(u=1000, s=600, e=100), out={"valueAddedCrPerYear": 300}, tags=["industrial_park", "sez", "export_zone"], req={"transport": ["road"], "supporting": [{"tag": "seaport", "count": 1, "radiusKm": 400, "why": "Export zones depend on port access"}]},
  risks=["Revenue foregone through tax concessions", "Enclave effects with limited local linkages"])

# ============================ TRANSPORT (lines) ============================
T = "Transportation"
def road(id, name, sub, capex, land, cap, pm, nox, co2, desc, months, basis):
    F(id, name, T, sub, "Transport", desc, "Connects places, facilities and markets; enables freight and passenger movement.", capex, months, land=land,
      opex=round(capex * 0.015, 3), cjobs=60, geometry="line", network="road", out={"freightCapacityMtpa": cap},
      em={"pm25": pm, "pm10": pm * 2.5, "nox": nox, "co": nox * 3, "voc": nox * 0.4, "co2": co2}, tags=["road", "transport"],
      risks=["Land acquisition along the corridor", "Induced traffic and emissions", "Must bridge rivers it crosses"],
      env="Vehicle emissions scale with traffic, which the model scales with population density.", basis=basis)
road("expressway", "Expressway (6-lane, access-controlled)", "Roads", 35, 9, 60, 0.8, 25, 3500, "High-speed access-controlled corridor.", 30, "≈₹30-70 crore/km for recent Indian expressways incl. land; 90 m ROW")
road("national_highway", "National highway (4-lane)", "Roads", 15, 6, 30, 0.6, 18, 2500, "Four-lane divided national highway.", 24, "MoRTH 4-laning ≈₹15-20 crore/km; 60 m ROW")
road("state_highway", "State highway (2-lane)", "Roads", 6, 3, 10, 0.3, 8, 1000, "Two-lane state highway.", 18, "Indicative ₹5-8 crore/km")
road("district_road", "District road", "Roads", 2, 1.5, 3, 0.12, 3, 350, "Major district road.", 12, "Indicative ₹1.5-3 crore/km")
road("local_road", "Local / rural road", "Roads", 0.7, 1, 1, 0.05, 1, 120, "Single-lane rural connectivity road.", 8, "PMGSY-type rural roads ≈₹0.5-0.8 crore/km")
F("bridge", "Major bridge", T, "Bridges", "Transport", "Major river bridge (road or rail) completing a network crossing.", "Allows road/rail lines to cross rivers; without a bridge the crossing is broken.", 500, 30,
  land=5, cjobs=600, tags=["bridge", "transport"], req={"maxRiverKm": 5}, risks=["Flood and scour risk", "Seismic design needed in Himalayan zones"], basis="Indicative ₹300-800 crore for a major bridge")
def rail(id, name, sub, capex, land, cap, months, basis, desc, em=None, extra=None):
    F(id, name, T, sub, "Railways", desc, "Moves bulk freight and passengers efficiently with low emissions per tonne-km.", capex, months, land=land, opex=round(capex * 0.02, 3), cjobs=80,
      geometry="line", network=extra or "rail", out={"freightCapacityMtpa": cap}, power=0.05, em=em or {"co2": 300}, tags=["rail", "transport"] if not extra else [extra, "transport", "urban_transit"],
      risks=["Long gestation", "Land acquisition", "Needs stations/terminals to be useful"], basis=basis)
rail("railway_track", "Railway line (double, electrified)", "Railways", 15, 4, 50, 36, "New line ≈₹10-20 crore/km", "Double-track electrified broad-gauge line.")
rail("freight_corridor", "Dedicated freight corridor", "Railways", 35, 5, 150, 48, "EDFC+WDFC ≈₹1.24 lakh crore / 2,843 km incl. land", "Heavy-haul freight-only double line.")
rail("high_speed_rail", "High-speed rail line", "Railways", 200, 4, 0, 72, "MAHSR ≈₹1.08 lakh crore / 508 km", "Standard-gauge 300+ km/h passenger line.")
rail("metro_line", "Metro line (elevated)", "Urban transit", 300, 1, 0, 48, "Indian metro elevated ≈₹200-350 crore/km", "Urban mass rapid transit line.", extra="metro")
F("inland_waterway", "Inland waterway development", T, "Waterways", "Transport", "Dredging, navigation aids and channel works along a navigable river.", "Low-cost bulk freight along rivers.", 2, 24,
  land=0, cjobs=30, geometry="line", network="waterway", out={"freightCapacityMtpa": 20}, em={"co2": 100, "nox": 1}, tags=["waterway", "transport"], req={"maxRiverKm": 10},
  risks=["Seasonal draft limitations", "Ecological impact of dredging"], basis="National Waterway-1 (Jal Marg Vikas) ≈₹5,000 crore for ~1,400 km incl. terminals")
F("railway_station", "Railway station", T, "Railways", "Railways", "Passenger and parcel station.", "Connects people and goods to the rail network.", 50, 18, land=5, cjobs=200, jobs=J(u=60, s=60, e=5),
  power=1, nodeOf=["rail", "road"], tags=["rail_node", "station"], req={"transport": ["road"]}, out={"passengerCapacityMpa": 10})
F("rail_junction", "Major railway junction & freight terminal", T, "Railways", "Railways", "Large junction with freight yards.", "Freight interchange hub.", 300, 30, land=60, cjobs=800, jobs=J(u=300, s=300, e=40),
  power=4, nodeOf=["rail", "road"], tags=["rail_node", "freight_terminal", "logistics"], out={"freightCapacityMtpa": 20, "passengerCapacityMpa": 30, "valueAddedCrPerYear": 80})
F("bus_terminal", "Bus terminal", T, "Road transport", "Transport", "Intercity bus terminal.", "Public transport access.", 80, 18, land=4, cjobs=150, jobs=J(u=150, s=60), power=1, nodeOf=["road"], tags=["bus", "transit"],
  out={"passengerCapacityMpa": 15})
F("domestic_airport", "Domestic airport", T, "Airports", "Aviation", "Regional airport (1 runway, terminal for ~5 Mppa).", "Air connectivity for passengers and express cargo.", 1500, 36, land=300, power=8, water=0.5,
  cjobs=1500, jobs=J(u=800, s=800, e=150), nodeOf=["air", "road"], tags=["airport"], out={"passengerCapacityMpa": 5, "valueAddedCrPerYear": 300}, req={"transport": ["road"]},
  em={"nox": 300, "co2": 150000, "pm25": 10, "co": 400}, risks=["Noise", "Land acquisition"], basis="Indicative greenfield regional airport costs")
F("international_airport", "International airport", T, "Airports", "Aviation", "Major international hub (2 runways, ~40 Mppa).", "Global passenger and air-cargo connectivity.", 20000, 60, land=2000, power=60, water=5,
  cjobs=10000, jobs=J(u=8000, s=8000, e=1500), nodeOf=["air", "road"], tags=["airport", "international_airport"], out={"passengerCapacityMpa": 40, "valueAddedCrPerYear": 4000},
  req={"transport": ["road"]}, em={"nox": 3000, "co2": 1500000, "pm25": 80, "co": 3000}, risks=["Noise", "Large land take", "Traffic demand risk"], basis="Navi Mumbai / Noida airports ≈₹10,000-17,000 crore for first phases")
F("seaport", "Seaport (major, 50 Mtpa)", T, "Ports", "Maritime", "Deep-water port with container and bulk berths.", "Gateway for exports and imports; anchors coastal industry.", 13000, 60, land=800, power=30, water=1,
  cjobs=5000, jobs=J(u=3000, s=2000, e=300), nodeOf=["sea", "road", "rail"], tags=["seaport", "port", "logistics"], out={"freightCapacityMtpa": 50, "valueAddedCrPerYear": 2500},
  req={"maxCoastKm": 8, "transport": ["road"]}, em={"pm25": 60, "pm10": 200, "nox": 2500, "so2": 1500, "co": 800, "co2": 200000},
  risks=["Coastal ecosystem impact", "Dredging and siltation", "Cyclone exposure"], basis="Vadhavan port ≈₹76,000 crore for ~298 Mtpa → ≈₹250 crore per Mtpa")
F("logistics_hub", "Multimodal logistics park", T, "Logistics", "Logistics", "Rail-road freight hub with warehousing and container handling.", "Cuts logistics costs by consolidating freight across modes.", 1000, 30, land=100, power=5,
  cjobs=1200, jobs=J(u=800, s=500, e=50), nodeOf=["rail", "road"], tags=["logistics", "logistics_hub", "rail_node"], out={"freightCapacityMtpa": 15, "valueAddedCrPerYear": 400}, public=True,
  basis="PM Gati Shakti MMLPs ≈₹1,000-1,500 crore each")
F("dry_port", "Dry port / inland container depot", T, "Logistics", "Logistics", "Inland customs-bonded container terminal linked by rail to ports.", "Extends port access inland.", 300, 24, land=40, power=2,
  cjobs=500, jobs=J(u=300, s=200, e=20), nodeOf=["rail", "road"], tags=["logistics", "dry_port", "rail_node"], out={"freightCapacityMtpa": 5, "valueAddedCrPerYear": 120})
F("waterway_terminal", "Inland waterway terminal", T, "Waterways", "Logistics", "River cargo terminal.", "Connects waterway freight to roads.", 150, 24, land=15, power=1, cjobs=200, jobs=J(u=120, s=60, e=5),
  nodeOf=["waterway", "road"], tags=["logistics", "waterway_terminal"], req={"maxRiverKm": 3}, out={"freightCapacityMtpa": 3})

# ============================ ENERGY & UTILITIES ============================
EN = "Energy & Utilities"
def plant(id, name, sub, mw, cf, capex, months, land, water, jobs, desc, em=None, inputs=None, req=None, tags=(), risks=(), basis="", tech=(), opex=None, env=""):
    va = round(mw * cf * 8.76 * 2.0, 0)  # ≈₹2/kWh net value added (indicative)
    F(id, name, EN, sub, "Power", desc, "Generates electricity for the grid.", capex, months, land=land, water=water, cjobs=int(mw * 2), jobs=jobs, opex=opex,
      out={"generationMW": mw, "capacityFactor": cf, "valueAddedCrPerYear": va}, inputs=inputs, req=req or {}, em=em or {}, tags=["power_plant"] + list(tags),
      risks=risks, basis=basis, tech=tech, env=env, econ="Value added assumed ≈₹2 per kWh generated (tariff minus fuel).")
coal_mwh = 1600 * 0.6 * 8760
plant("coal_plant", "Coal power plant (2 x 800 MW supercritical)", "Thermal", 1600, 0.6, 12800, 54, 500, 80, J(u=250, s=400, e=150),
      "Supercritical pulverised-coal plant with electrostatic precipitators (no flue-gas desulphurisation).",
      em={"pm25": round(coal_mwh * 0.00015), "pm10": round(coal_mwh * 0.0003), "nox": round(coal_mwh * 0.0025), "so2": round(coal_mwh * 0.0065), "co": round(coal_mwh * 0.0002), "voc": 300, "co2": round(coal_mwh * 0.95)},
      inputs={"coal": round(coal_mwh * 0.65)}, req={"transport": ["rail"], "water": True}, tags=["thermal", "coal_power"], tech=["supercritical boilers"],
      risks=["High SO2/NOx/PM and CO2 emissions", "Coal supply logistics", "Stranded-asset risk under climate policy", "Water stress"], basis="≈₹8 crore/MW (CEA benchmark range); 0.65 t coal/MWh; 0.95 t CO2/MWh; Indian coal ~0.5% S", env="Major source of SO2, NOx, PM and CO2.")
plant("coal_plant_fgd", "Coal power plant with FGD (2 x 800 MW)", "Thermal", 1600, 0.6, 13600, 60, 520, 85, J(u=260, s=420, e=160),
      "Supercritical coal plant fitted with flue-gas desulphurisation and SCR for NOx.",
      em={"pm25": round(coal_mwh * 0.0001), "pm10": round(coal_mwh * 0.0002), "nox": round(coal_mwh * 0.0008), "so2": round(coal_mwh * 0.0007), "co": round(coal_mwh * 0.0002), "voc": 300, "co2": round(coal_mwh * 0.96)},
      inputs={"coal": round(coal_mwh * 0.66), "limestone": round(coal_mwh * 0.012)}, req={"transport": ["rail"], "water": True}, tags=["thermal", "coal_power"],
      risks=["High CO2", "Coal and limestone logistics", "Stranded-asset risk"], basis="FGD+SCR adds ≈₹0.5 crore/MW; ~90% SO2 removal")
gas_mwh = 750 * 0.4 * 8760
plant("gas_plant", "Gas combined-cycle plant (750 MW)", "Thermal", 750, 0.4, 3750, 36, 40, 10, J(u=60, s=120, e=60), "Combined-cycle gas turbine plant; flexible output.",
      em={"pm25": 50, "pm10": 60, "nox": round(gas_mwh * 0.0004), "so2": 10, "co": round(gas_mwh * 0.0002), "voc": 50, "co2": round(gas_mwh * 0.37)}, inputs={"natural_gas": round(gas_mwh * 0.13)},
      tags=["thermal", "gas_power", "flexible"], risks=["Imported LNG price volatility", "Low utilisation if gas is scarce"], basis="≈₹5 crore/MW; 0.13 t LNG/MWh; 0.37 t CO2/MWh")
plant("nuclear_plant", "Nuclear power plant (2 x 700 MW PHWR)", "Nuclear", 1400, 0.8, 21000, 84, 400, 90, J(u=200, s=500, e=450, r=50),
      "Pressurised heavy-water reactor units of the indigenous 700 MWe design.", inputs={"uranium": 170}, em={"co2": 15000},
      req={"water": True, "tech": {"nuclear": 2}, "supporting": [{"tag": "research_university", "count": 1, "radiusKm": 0, "why": "Nuclear operations need a deep pool of nuclear engineers and regulators"}]},
      tags=["nuclear", "low_carbon", "firm_power"], tech=["PHWR"], risks=["Long construction and cost overrun risk", "Spent-fuel management", "Public acceptance and siting", "Uranium import dependence"],
      basis="Indian 700 MWe PHWR fleet-mode projects ≈₹12-15 crore/MW; ~150 t natural U per GWe-year")
plant("hydro_plant", "Hydroelectric plant (1,000 MW)", "Hydro", 1000, 0.4, 10000, 96, 2000, 0, J(u=150, s=200, e=100),
      "Storage hydro project with dam and reservoir.", em={"co2": 30000}, req={"mountain": True, "maxRiverKm": 5, "grid": False},
      tags=["hydro", "renewable", "low_carbon", "flexible"], risks=["Displacement and ecological damage", "Seismic and glacial-lake flood risk", "Long gestation"],
      basis="≈₹10 crore/MW for large hydro in India", env="Reservoir floods land; alters river flows.")
plant("small_hydro", "Small hydro plant (25 MW)", "Hydro", 25, 0.45, 225, 36, 20, 0, J(u=10, s=10, e=5), "Run-of-river small hydro.", req={"mountain": True, "maxRiverKm": 3, "grid": False},
      tags=["hydro", "renewable", "low_carbon"], risks=["Seasonal flow variation"], basis="≈₹9 crore/MW")
plant("solar_farm", "Solar PV farm (500 MW)", "Solar", 500, 0.2, 2000, 12, 1000, 0.5, J(u=30, s=20, e=8),
      "Utility-scale ground-mounted photovoltaic plant.", inputs={"solar_module": 500 / 25}, req={"grid": False, "states": SOLAR_STATES, "statesSource": "MNRE / NISE solar potential (high-irradiance states, indicative)"},
      tags=["solar", "renewable", "low_carbon", "variable"], risks=["Variable output (needs storage/flexible supply)", "Large land footprint", "Module import dependence"],
      basis="≈₹4 crore/MW (2023-24 tenders); 19-21% CUF; ~2 ha/MW; module replacement ~4%/yr")
plant("rooftop_solar", "Rooftop solar programme (100 MW)", "Solar", 100, 0.17, 500, 12, 0, 0.05, J(u=150, s=200, e=20), "Distributed rooftop PV on homes and buildings across a district.",
      inputs={"solar_module": 4}, req={"grid": False, "urban": True}, tags=["solar", "renewable", "low_carbon", "variable", "distributed"], basis="≈₹5 crore/MW (PM Surya Ghar scale)")
plant("wind_farm", "Onshore wind farm (300 MW)", "Wind", 300, 0.3, 1950, 18, 600, 0, J(u=30, s=40, e=15), "Onshore wind turbines (3-4 MW class).",
      req={"grid": False, "states": WIND_STATES, "statesSource": NIWE}, tags=["wind", "renewable", "low_carbon", "variable"], risks=["Variable output", "Land and bird-impact concerns"], basis="≈₹6.5 crore/MW; ~30% CUF in good sites")
plant("offshore_wind", "Offshore wind farm (500 MW)", "Wind", 500, 0.38, 9000, 48, 0, 0, J(u=40, s=80, e=40), "Fixed-bottom offshore wind project.",
      req={"grid": False, "maxCoastKm": 15, "states": ["Gujarat", "Tamil Nadu"], "statesSource": "MNRE offshore wind zones (Gujarat, Tamil Nadu)", "tech": {"energy": 3}},
      tags=["wind", "renewable", "low_carbon", "variable", "offshore"], risks=["High cost and immature domestic supply chain", "Cyclones"], basis="≈₹18 crore/MW (MNRE VGF estimates)")
plant("geothermal_plant", "Geothermal plant (20 MW)", "Geothermal", 20, 0.7, 500, 48, 30, 0.2, J(u=10, s=15, e=10), "Binary-cycle geothermal plant.",
      req={"grid": False, "states": ["Ladakh", "Himachal Pradesh", "Uttarakhand", "Chhattisgarh", "Gujarat", "Maharashtra", "Jharkhand"], "statesSource": "Geological Survey of India geothermal provinces (indicative)", "tech": {"energy": 2}},
      tags=["geothermal", "renewable", "low_carbon", "firm_power"], risks=["Resource uncertainty: India has little proven geothermal capacity", "Drilling risk"], basis="Pilot-scale indicative ₹25 crore/MW")
F("waste_to_energy", "Waste-to-energy plant (1,000 TPD)", EN, "Waste", "Waste management", "Mass-burn incinerator with energy recovery.", "Disposes of municipal waste and generates power.", 450, 30, land=10, water=2,
  cjobs=500, jobs=J(u=60, s=50, e=15), out={"generationMW": 15, "capacityFactor": 0.6, "wasteTPD": 1000, "valueAddedCrPerYear": 60},
  em={"pm25": 30, "pm10": 40, "nox": 600, "so2": 150, "co": 200, "co2": 350000}, tags=["waste", "power_plant", "waste_processing"], risks=["Poor performance with wet unsegregated waste", "Dioxin emissions if poorly operated"],
  basis="Indicative Indian WtE plants")
F("battery_storage", "Battery energy storage (250 MW / 1,000 MWh)", EN, "Storage", "Power", "Grid-scale lithium-ion battery storage.", "Firms variable renewable output and improves grid reliability.", 1800, 12, land=8,
  cjobs=150, jobs=J(s=15, e=10), out={"storageMWh": 1000, "valueAddedCrPerYear": 80}, inputs={"battery_cell": 1000000 / 10}, req={"grid": False}, tags=["storage", "battery_storage"],
  risks=["Battery degradation (~10-15 year life)", "Imported cell dependence"], basis="≈₹1.8 crore/MWh (2024 BESS tenders)")
F("pumped_hydro", "Pumped hydro storage (1,200 MW / 7,200 MWh)", EN, "Storage", "Power", "Two-reservoir pumped storage.", "Long-duration storage for renewable-heavy grids.", 7200, 60, land=400,
  cjobs=2000, jobs=J(u=40, s=60, e=30), out={"storageMWh": 7200, "valueAddedCrPerYear": 300}, req={"mountain": True, "grid": False}, tags=["storage", "pumped_hydro", "flexible"],
  risks=["Site-specific geology", "Ecological impacts"], basis="≈₹6 crore/MW")
F("green_hydrogen", "Green hydrogen electrolyser (100 MW)", EN, "Hydrogen", "Clean energy", "PEM/alkaline electrolysers producing hydrogen from renewable electricity.", "Decarbonises fertiliser, refining and steel.", 1000, 24, land=10,
  power=100, water=0.6, cjobs=400, jobs=J(s=40, e=30, r=5), out={"goods": [{"good": "green_hydrogen", "qtyPerYear": 15000}], "valueAddedCrPerYear": 200},
  req={"tech": {"energy": 2}, "minGridReliability": 0.8}, tags=["hydrogen", "clean_energy"], tech=["electrolysers"], risks=["High cost vs grey hydrogen", "Needs cheap renewable power"],
  basis="≈₹10 crore/MW electrolyser incl. balance of plant; ~55 kWh/kg; ~9 L water/kg")
F("substation", "Electricity substation (400/220 kV)", EN, "Grid", "Power", "Transformer substation linking transmission to district distribution.", "Electricity reaches a district only through a substation or distribution network.", 300, 18,
  land=20, cjobs=200, jobs=J(s=30, e=10), out={"transmissionMW": 1500, "distributionMW": 1000}, nodeOf=["power"], tags=["substation", "grid"], req={"grid": False}, basis="Indicative ₹250-400 crore for a 400 kV substation")
F("distribution_network", "District distribution network", EN, "Grid", "Power", "11 kV/LT distribution lines, transformers and smart meters for a district.", "Connects households and businesses to the grid.", 600, 24,
  land=0, cjobs=1500, jobs=J(u=200, s=300, e=30), out={"distributionMW": 800}, nodeOf=["power"], tags=["distribution", "grid"], req={"grid": False}, basis="Indicative; RDSS-scale district investment")
F("transmission_line", "Transmission line (400 kV)", EN, "Grid", "Power", "Double-circuit 400 kV overhead line.", "Moves bulk power between plants and substations; joins grid islands.", 1.5, 18, land=0.5,
  opex=0.02, cjobs=20, geometry="line", network="power", out={"transmissionMW": 1500}, tags=["transmission", "grid"], req={"grid": False}, risks=["Right-of-way disputes"], basis="≈₹1-2 crore/km for 400 kV D/C")
F("hvdc_line", "HVDC transmission line (±800 kV)", EN, "Grid", "Power", "Ultra-high-voltage DC corridor for long-distance bulk transfer.", "Low-loss long-distance transfer of renewable power.", 4, 36, land=0.6, opex=0.05,
  cjobs=40, geometry="line", network="power", out={"transmissionMW": 6000}, tags=["transmission", "grid", "hvdc"], req={"grid": False, "tech": {"energy": 2}}, basis="Indicative ₹3-5 crore/km")
F("water_treatment", "Water treatment plant (200 MLD)", EN, "Water", "Water", "Conventional surface-water treatment plant.", "Supplies safe drinking water to a district.", 180, 24, land=15, power=4,
  cjobs=300, jobs=J(u=40, s=40, e=8), out={"waterSupplyMLD": 200}, req={"maxRiverKm": 40}, tags=["water", "water_supply"], risks=["Raw-water source reliability"], basis="≈₹0.9 crore/MLD")
F("desalination", "Desalination plant (100 MLD, RO)", EN, "Water", "Water", "Seawater reverse-osmosis plant.", "Drought-proof water for coastal cities.", 900, 30, land=10, power=17,
  cjobs=400, jobs=J(u=30, s=40, e=10), out={"waterSupplyMLD": 100}, req={"maxCoastKm": 5}, tags=["water", "water_supply", "desalination"], em={"co2": 0},
  risks=["High electricity use (~4 kWh/m³)", "Brine discharge harms marine ecology"], basis="Chennai Nemmeli 150 MLD ≈₹1,259 crore")
F("sewage_treatment", "Sewage treatment plant (100 MLD)", EN, "Sanitation", "Water", "Activated-sludge/SBR sewage treatment plant.", "Treats urban sewage; protects rivers and health.", 180, 24, land=12, power=3,
  cjobs=250, jobs=J(u=30, s=30, e=6), out={"sewageTreatMLD": 100}, tags=["sanitation", "sewage"], basis="≈₹1.5-2 crore/MLD (Namami Gange STPs)")
F("water_pipeline", "Bulk water pipeline", EN, "Water", "Water", "Trunk water main (≈1.5 m diameter).", "Moves treated water between districts (joins water networks).", 8, 18, land=0.3, power=0.02, opex=0.1,
  cjobs=20, geometry="line", network="water", out={"waterSupplyMLD": 0}, tags=["water", "pipeline"], basis="Indicative ₹6-10 crore/km")
F("solid_waste_plant", "Integrated solid-waste processing (1,000 TPD)", EN, "Waste", "Waste management", "Segregation, composting, material recovery and engineered landfill.", "Processes municipal solid waste.", 300, 18, land=40, power=2,
  cjobs=300, jobs=J(u=300, s=40, e=5), out={"wasteTPD": 1000, "valueAddedCrPerYear": 20}, em={"pm10": 20, "co2": 50000, "voc": 50}, tags=["waste", "waste_processing"], basis="Indicative")

# ============================ AGRICULTURE & FOOD ============================
AG = "Agriculture & Food"
F("farm_cluster", "Modern farm cluster (1,000 ha)", AG, "Farming", "Agriculture", "Consolidated, mechanised farm cluster with improved seeds.", "Raises food production per hectare and per worker.", 50, 12, land=1000, water=14, power=0.5,
  cjobs=100, jobs=J(u=350, s=40, e=2), out={"goods": [{"good": "food_grain", "qtyPerYear": 4500}], "valueAddedCrPerYear": 9}, inputs={"fertilizer": 250}, tags=["agriculture", "farm"],
  em={"pm25": 3, "nox": 3, "co2": 1500}, risks=["Groundwater depletion without efficient irrigation", "Soil degradation with fertiliser overuse"], basis="~4.5 t/ha irrigated cereal yields; ~250 kg/ha fertiliser")
F("irrigation_canal", "Canal irrigation system (10,000 ha)", AG, "Irrigation", "Agriculture", "Distributary canal network from a river source.", "Increases yields on legacy farmland and enables double cropping.", 500, 36, land=200,
  cjobs=1500, jobs=J(u=100, s=30, e=5), out={"irrigationHa": 10000}, req={"maxRiverKm": 30}, tags=["irrigation", "agriculture"], risks=["Waterlogging and salinity", "Upstream-downstream water conflicts"], basis="≈₹5 lakh/ha")
F("micro_irrigation", "Drip & sprinkler irrigation project (10,000 ha)", AG, "Irrigation", "Agriculture", "Micro-irrigation for water-efficient farming.", "Raises yields while saving water.", 150, 12, land=0, power=1,
  cjobs=600, jobs=J(u=50, s=20), out={"irrigationHa": 10000}, tags=["irrigation", "agriculture", "water_efficient"], basis="≈₹1.5 lakh/ha (PMKSY 'Per Drop More Crop')")
F("greenhouse", "Greenhouse complex (50 ha)", AG, "Farming", "Agriculture", "Climate-controlled horticulture.", "High-value vegetables, flowers and seedlings.", 100, 12, land=50, power=3, water=0.5,
  cjobs=150, jobs=J(u=300, s=50, e=5), out={"goods": [{"good": "processed_food", "qtyPerYear": 60}], "valueAddedCrPerYear": 40}, tags=["agriculture", "horticulture"])
F("agri_research", "Agricultural research centre", AG, "Research", "Agriculture", "Crop science, seeds, soil and water research institute.", "Raises agricultural productivity over time.", 300, 24, land=200, power=2,
  cjobs=300, jobs=J(u=100, s=100, e=30, r=150), out={"researchPoints": {"agriculture": 40}, "foodYieldBoost": 0.02, "valueAddedCrPerYear": 30}, tags=["research", "agri_research"], basis="ICAR institute scale")
F("fertilizer_plant", "Fertiliser plant (urea, 1.27 Mtpa)", AG, "Agri-inputs", "Chemicals", "Gas-based ammonia-urea complex.", "Reduces fertiliser imports.", 8300, 42, land=250, power=40, water=20,
  cjobs=5000, jobs=J(u=300, s=500, e=200), out={"goods": [{"good": "fertilizer", "qtyPerYear": 1270000}], "valueAddedCrPerYear": 1200}, inputs={"natural_gas": 700000},
  req={"transport": ["rail"], "tech": {"heavy_industry": 1}}, em={"pm25": 200, "pm10": 400, "nox": 1500, "so2": 100, "co": 300, "voc": 200, "co2": 2200000}, tags=["fertilizer", "chemicals", "manufacturing"],
  public=False, risks=["Dependence on imported LNG", "Ammonia safety"], basis="HURL urea plants ≈₹8,300 crore each")
F("food_processing", "Food processing factory", AG, "Food industry", "Food processing", "Milling, packaging and processed-food manufacturing.", "Adds value to farm output and reduces waste.", 300, 18, land=15, power=6, water=2,
  cjobs=500, jobs=J(u=800, s=200, e=20), inputs={"food_grain": 100000}, out={"goods": [{"good": "processed_food", "qtyPerYear": 600}], "valueAddedCrPerYear": 180},
  req={"transport": ["road"]}, em={"pm25": 5, "co2": 15000}, tags=["food_processing", "manufacturing"], public=False)
F("cold_storage", "Cold storage (10,000 t)", AG, "Supply chain", "Logistics", "Refrigerated warehouse for perishables.", "Reduces post-harvest losses.", 30, 9, land=2, power=1.5, cjobs=60, jobs=J(u=30, s=10),
  out={"foodYieldBoost": 0.002, "valueAddedCrPerYear": 6}, req={"transport": ["road"]}, tags=["cold_chain", "agri_logistics"], public=False)
F("grain_warehouse", "Grain silo / warehouse (50,000 t)", AG, "Supply chain", "Logistics", "Modern steel silo complex.", "Strategic food reserves; reduces storage losses.", 100, 12, land=5, power=0.5, cjobs=100, jobs=J(u=40, s=10),
  out={"foodYieldBoost": 0.003, "valueAddedCrPerYear": 8}, req={"transport": ["road"]}, tags=["grain_storage", "agri_logistics", "food_security"])
F("dairy_plant", "Dairy processing plant (1 million L/day)", AG, "Food industry", "Dairy", "Milk chilling, processing and packaging.", "Organises dairy value chains for farmers.", 500, 18, land=20, power=8, water=3,
  cjobs=600, jobs=J(u=600, s=200, e=20), out={"goods": [{"good": "milk", "qtyPerYear": 365000}], "valueAddedCrPerYear": 300}, req={"transport": ["road"]}, tags=["dairy", "food_processing"], public=False,
  em={"co2": 20000})
F("fishery_harbour", "Fishing harbour & aquaculture hub", AG, "Fisheries", "Fisheries", "Fishing harbour with landing, auction and cold chain.", "Supports fishers' livelihoods and seafood exports.", 200, 24, land=15, power=2,
  cjobs=400, jobs=J(u=1500, s=100), out={"goods": [{"good": "fish", "qtyPerYear": 30000}], "valueAddedCrPerYear": 150}, req={"maxCoastKm": 3}, tags=["fisheries"], risks=["Overfishing"])
F("agri_logistics", "Agricultural logistics & market centre", AG, "Supply chain", "Logistics", "Mandi with sorting, grading and aggregation.", "Better price discovery and reduced wastage.", 150, 12, land=10, power=1,
  cjobs=200, jobs=J(u=200, s=50, e=5), out={"foodYieldBoost": 0.004, "valueAddedCrPerYear": 25}, req={"transport": ["road"]}, tags=["agri_logistics"])

# ============================ HEALTH & EDUCATION ============================
HE = "Healthcare & Education"
F("primary_school", "Primary school (500 seats)", HE, "Schools", "Education", "Government primary school (classes 1-5).", "Foundational literacy and numeracy.", 3, 12, land=0.5, power=0.02, opex=1.5,
  cjobs=30, jobs=J(s=18, u=4), out={"schoolSeats": 500}, tags=["school", "education"], basis="Indicative ₹2-4 crore per school building")
F("secondary_school", "Secondary school (1,200 seats)", HE, "Schools", "Education", "Secondary & higher-secondary school with labs.", "Builds the base for skilled workforce and higher education.", 15, 18, land=2, power=0.1, opex=5,
  cjobs=80, jobs=J(s=55, u=10), out={"schoolSeats": 1200, "graduatesPerYear": {"skilled": 40}}, tags=["school", "education", "secondary_school"])
F("vocational_institute", "Vocational training institute (ITI)", HE, "Skills", "Education", "Industrial training institute.", "Produces skilled technicians for industry.", 30, 18, land=3, power=0.3, opex=6,
  cjobs=80, jobs=J(s=40, e=5, u=10), out={"graduatesPerYear": {"skilled": 800}}, tags=["vocational", "education", "skills"])
F("university", "University", HE, "Higher education", "Education", "Multi-disciplinary university (~10,000 students).", "Graduates and research capacity.", 1000, 36, land=100, power=6, water=1.5, opex=150,
  cjobs=1500, jobs=J(u=300, s=400, e=100, r=500), out={"higherEdSeats": 10000, "graduatesPerYear": {"skilled": 1500, "engineer": 300, "scientist": 150}, "researchPoints": {"materials": 5, "biotech_pharma": 5, "computing_ai": 5, "agriculture": 5}},
  tags=["university", "education", "research"], basis="Indicative new central university scale")
F("engineering_college", "Engineering college / institute of technology", HE, "Higher education", "Education", "Engineering institute (~4,000 students).", "Supplies engineers to every advanced industry.", 600, 36, land=80, power=5, water=1, opex=120,
  cjobs=1000, jobs=J(u=200, s=250, e=100, r=300), out={"higherEdSeats": 4000, "graduatesPerYear": {"engineer": 1000, "scientist": 80}, "researchPoints": {"electronics": 10, "semiconductor": 4, "computing_ai": 10, "heavy_industry": 8, "robotics_automation": 6}},
  tags=["engineering_college", "university", "education", "research"], basis="New IIT campuses ≈₹1,000-2,000 crore; smaller institutes less")
F("medical_college", "Medical college with teaching hospital", HE, "Higher education", "Health & education", "Medical college (150 MBBS seats/yr) with 600-bed hospital.", "Trains doctors and provides tertiary care.", 600, 36, land=25, power=6, water=1, opex=150,
  cjobs=1200, jobs=J(u=500, s=600, r=80, m=400), out={"higherEdSeats": 900, "hospitalBeds": 600, "graduatesPerYear": {"medical": 150}, "researchPoints": {"biotech_pharma": 8}},
  tags=["medical_college", "hospital", "education", "health"], basis="NMC norms; ₹325-600 crore per new college")
F("research_university", "Research university", HE, "Higher education", "Research", "Research-intensive university with doctoral programmes and national labs.", "Frontier research and scientists.", 3000, 48, land=200, power=20, water=2, opex=400,
  cjobs=3000, jobs=J(u=500, s=800, e=400, r=1500), out={"higherEdSeats": 8000, "graduatesPerYear": {"scientist": 600, "engineer": 400}, "researchPoints": {"semiconductor": 10, "computing_ai": 20, "biotech_pharma": 15, "materials": 15, "quantum": 6, "nuclear": 8, "aerospace_space": 8, "energy": 10}},
  tags=["research_university", "university", "research", "education"], req={"supporting": [{"tag": "engineering_college", "count": 1, "radiusKm": 0, "why": "Research universities draw students from undergraduate engineering programmes"}]})
F("phc", "Primary health centre", HE, "Healthcare", "Health", "PHC with outpatient care, maternal health, immunisation and 6 beds.", "First point of public healthcare (IPHS: one per 30,000 people in plains).", 2, 12, land=0.5, power=0.05, opex=1.2,
  cjobs=30, jobs=J(u=5, s=8, m=4), out={"primaryCareCoverage": 30000, "hospitalBeds": 6}, tags=["phc", "health", "primary_care"], basis="IPHS norms")
F("chc", "Community health centre (30 beds)", HE, "Healthcare", "Health", "First referral unit with specialists.", "Secondary care for ~120,000 people.", 15, 18, land=2, power=0.3, opex=8,
  cjobs=80, jobs=J(u=20, s=30, m=25), out={"primaryCareCoverage": 120000, "hospitalBeds": 30}, tags=["chc", "health", "primary_care"])
F("district_hospital", "District hospital (300 beds)", HE, "Healthcare", "Health", "Multi-specialty district hospital.", "Secondary care, surgery and emergency services.", 300, 30, land=8, power=3, water=0.3, opex=60,
  cjobs=600, jobs=J(u=200, s=300, m=300), out={"hospitalBeds": 300}, tags=["hospital", "health"], basis="≈₹1 crore per bed incl. equipment")
F("tertiary_hospital", "Tertiary super-specialty hospital (750 beds)", HE, "Healthcare", "Health", "AIIMS-type institute.", "Advanced care, training and medical research.", 1500, 48, land=80, power=10, water=0.8, opex=400,
  cjobs=2000, jobs=J(u=800, s=900, r=100, m=900), out={"hospitalBeds": 750, "graduatesPerYear": {"medical": 100}, "researchPoints": {"biotech_pharma": 10}}, tags=["hospital", "health", "tertiary_hospital"],
  req={"transport": ["road"]}, basis="New AIIMS ≈₹1,200-1,800 crore")
F("pharma_factory", "Pharmaceutical formulations factory", HE, "Pharmaceuticals", "Pharmaceuticals", "Tablets, capsules and injectables manufacturing (GMP).", "Affordable medicines and exports.", 500, 24, land=20, power=8, water=1.5,
  cjobs=600, jobs=J(u=300, s=400, e=80, r=40), inputs={"api": 400}, out={"goods": [{"good": "pharmaceuticals", "qtyPerYear": 1200}], "valueAddedCrPerYear": 500},
  req={"transport": ["road"], "tech": {"biotech_pharma": 1}}, em={"voc": 50, "co2": 20000}, tags=["pharma", "manufacturing"], public=False, risks=["Regulatory compliance (GMP) failures", "Effluent pollution"])
F("api_plant", "Active pharmaceutical ingredient (API) plant", HE, "Pharmaceuticals", "Pharmaceuticals", "Bulk drug / API chemical synthesis plant.", "Reduces dependence on imported APIs.", 1000, 30, land=40, power=15, water=4,
  cjobs=1000, jobs=J(u=300, s=400, e=150, r=80), inputs={"chemicals": 300}, out={"goods": [{"good": "api", "qtyPerYear": 900}], "valueAddedCrPerYear": 450},
  req={"transport": ["road"], "tech": {"biotech_pharma": 2}}, em={"voc": 300, "co2": 60000, "nox": 50}, tags=["pharma", "api", "manufacturing", "chemicals_user"], public=False,
  risks=["Highly polluting effluents", "Price competition from imports"], basis="PLI bulk-drug parks scale")
F("medical_equipment", "Medical equipment manufacturing", HE, "Medical devices", "Medical devices", "Diagnostic imaging, monitors and consumables.", "Reduces medical-device import dependence.", 300, 24, land=10, power=4,
  cjobs=400, jobs=J(u=200, s=300, e=100, r=20), inputs={"electronic_components": 120}, out={"goods": [{"good": "medical_devices", "qtyPerYear": 500}], "valueAddedCrPerYear": 180},
  req={"tech": {"electronics": 1}}, tags=["medical_devices", "manufacturing"], public=False)
F("biotech_research", "Biotechnology research centre", HE, "Research", "Biotechnology", "Genomics, vaccines and biologics research.", "Builds biotech capability.", 400, 24, land=15, power=5, opex=60,
  cjobs=400, jobs=J(u=50, s=150, e=40, r=300), out={"researchPoints": {"biotech_pharma": 40}}, tags=["biotech_research", "research"])
F("pharma_research", "Pharmaceutical R&D centre", HE, "Research", "Pharmaceuticals", "Drug discovery and clinical research centre.", "New molecules and process innovation.", 500, 24, land=10, power=4, opex=80,
  cjobs=400, jobs=J(u=50, s=150, e=60, r=350), out={"researchPoints": {"biotech_pharma": 50}}, tags=["pharma_research", "research"], req={"supporting": [{"tag": "pharma", "count": 1, "radiusKm": 0, "why": "R&D centres are most productive near manufacturing that can commercialise results"}]})
F("biologics_plant", "Biologics & vaccine manufacturing", HE, "Pharmaceuticals", "Biotechnology", "Bioreactor-based vaccine and biologics plant.", "Vaccine security and high-value exports.", 2000, 36, land=30, power=20, water=3,
  cjobs=1200, jobs=J(u=200, s=500, e=200, r=200), inputs={"chemicals": 200}, out={"goods": [{"good": "pharmaceuticals", "qtyPerYear": 2500}], "valueAddedCrPerYear": 1500},
  req={"tech": {"biotech_pharma": 3}, "minGridReliability": 0.95}, tags=["biotech", "pharma", "manufacturing"], public=False)

# ============================ HEAVY INDUSTRY & MINING ============================
HI = "Heavy Industry & Mining"
def mine(id, name, good, qty, capex, states, desc, em, jobs, power=5, basis=""):
    F(id, name, HI, "Mining", "Mining", desc, "Domestic raw-material supply.", capex, 30, land=1500, power=power, water=2, cjobs=1500, jobs=jobs,
      out={"goods": [{"good": good, "qtyPerYear": qty}], "valueAddedCrPerYear": round(qty * 0.4 * {"coal": 0.0002, "iron_ore": 0.0004, "limestone": 0.00005, "bauxite": 0.00015, "rare_earths": 0.05, "lithium": 0.1}[good], 0)},
      req={"states": states, "statesSource": IBM, "transport": ["rail"]}, em=em, tags=["mining", good + "_mine"], public=False,
      risks=["Displacement and forest loss", "Dust and water pollution", "Requires geological exploration (deposit data not bundled)"], basis=basis or "Indicative")
mine("coal_mine", "Coal mine (10 Mtpa opencast)", "coal", 10000000, 1500, COAL_STATES, "Opencast coal mine with washery.", {"pm10": 3000, "pm25": 600, "co2": 300000, "nox": 500}, J(u=1500, s=800, e=100))
mine("iron_ore_mine", "Iron ore mine (10 Mtpa)", "iron_ore", 10000000, 1200, ["Odisha", "Chhattisgarh", "Karnataka", "Jharkhand", "Goa"], "Mechanised iron-ore mine.", {"pm10": 2500, "pm25": 400, "co2": 150000}, J(u=1000, s=500, e=80))
mine("limestone_quarry", "Limestone quarry (5 Mtpa)", "limestone", 5000000, 300, ["Rajasthan", "Madhya Pradesh", "Andhra Pradesh", "Telangana", "Karnataka", "Gujarat", "Chhattisgarh", "Tamil Nadu", "Meghalaya"], "Limestone quarry for cement and steel.", {"pm10": 1500, "pm25": 200, "co2": 30000}, J(u=400, s=150, e=20))
mine("bauxite_mine", "Bauxite mine (3 Mtpa)", "bauxite", 3000000, 600, ["Odisha", "Gujarat", "Jharkhand", "Maharashtra", "Chhattisgarh", "Andhra Pradesh"], "Bauxite mine for alumina.", {"pm10": 1200, "pm25": 200, "co2": 40000}, J(u=500, s=200, e=30))
mine("rare_earth_plant", "Rare-earth mining & separation", "rare_earths", 5000, 2500, ["Kerala", "Tamil Nadu", "Odisha", "Andhra Pradesh"], "Beach-sand monazite mining and rare-earth separation.", {"pm10": 200, "co2": 50000, "so2": 100}, J(u=300, s=300, e=150, r=50), power=20,
     basis="India has monazite beach-sand deposits; separation capacity is limited (IREL)")
mine("lithium_refinery", "Lithium extraction & refining", "lithium", 20000, 4000, ["Jammu & Kashmir", "Karnataka", "Rajasthan", "Chhattisgarh"], "Hard-rock lithium mining and refining to carbonate/hydroxide.", {"pm10": 300, "co2": 300000, "so2": 200}, J(u=300, s=400, e=150, r=40), power=40,
     basis="GSI reported lithium resources (J&K Reasi, Karnataka); commercial viability unproven")
F("steel_plant", "Integrated steel plant (5 Mtpa)", HI, "Metals", "Steel", "Blast-furnace / basic-oxygen integrated steelworks.", "Steel for construction, vehicles and machinery.", 35000, 54, land=1500, power=300, water=60,
  cjobs=15000, jobs=J(u=3000, s=4000, e=900, r=50), inputs={"iron_ore": 8000000, "coal": 4000000, "limestone": 1500000}, out={"goods": [{"good": "steel", "qtyPerYear": 5000000}], "valueAddedCrPerYear": 8000},
  req={"transport": ["rail"], "tech": {"heavy_industry": 2}}, em={"pm25": 4000, "pm10": 10000, "nox": 8000, "so2": 9000, "co": 60000, "voc": 2000, "co2": 12500000}, tags=["steel", "heavy_industry", "manufacturing"],
  public=False, risks=["Very high CO2 (~2.5 t/t steel)", "Air pollution", "Raw-material logistics"], basis="≈₹7,000 crore per Mtpa")
F("cement_plant", "Cement plant (3 Mtpa)", HI, "Building materials", "Cement", "Dry-process cement plant with kiln.", "Construction materials.", 2000, 30, land=250, power=30, water=1.5,
  cjobs=2500, jobs=J(u=400, s=400, e=80), inputs={"limestone": 4500000, "coal": 450000}, out={"goods": [{"good": "cement", "qtyPerYear": 3000000}], "valueAddedCrPerYear": 600},
  req={"transport": ["rail"]}, em={"pm25": 600, "pm10": 1500, "nox": 4500, "so2": 1000, "co": 3000, "co2": 2400000}, tags=["cement", "heavy_industry", "manufacturing"], public=False, basis="≈₹650 crore per Mtpa; ~0.8 t CO2/t cement")
F("aluminium_smelter", "Aluminium smelter & refinery (0.5 Mtpa)", HI, "Metals", "Aluminium", "Alumina refinery and electrolytic smelter.", "Aluminium for power lines, vehicles, packaging.", 15000, 48, land=800, power=750, water=20,
  cjobs=6000, jobs=J(u=1000, s=1500, e=400), inputs={"bauxite": 2500000, "coal": 300000}, out={"goods": [{"good": "aluminium", "qtyPerYear": 500000}], "valueAddedCrPerYear": 3500},
  req={"transport": ["rail"], "tech": {"heavy_industry": 2}, "minGridReliability": 0.9}, em={"pm25": 500, "pm10": 1000, "so2": 4000, "nox": 1000, "co2": 1500000}, tags=["aluminium", "heavy_industry", "manufacturing"], public=False,
  risks=["Huge electricity demand (~13-15 MWh/t)", "Red-mud disposal"])
F("oil_refinery", "Oil refinery (10 Mtpa)", HI, "Petroleum", "Refining", "Crude-oil refinery producing fuels.", "Fuel security.", 30000, 54, land=1000, power=100, water=25,
  cjobs=10000, jobs=J(u=800, s=1500, e=600, r=30), inputs={"crude_oil": 10000000}, out={"goods": [{"good": "refined_fuels", "qtyPerYear": 9300000}], "valueAddedCrPerYear": 6000},
  req={"maxCoastKm": 150, "transport": ["rail"], "tech": {"heavy_industry": 2}}, em={"pm25": 500, "pm10": 800, "nox": 4000, "so2": 6000, "co": 1500, "voc": 5000, "co2": 3000000}, tags=["refinery", "heavy_industry", "manufacturing"],
  public=False, risks=["Crude import dependence", "VOC and SO2 emissions", "Fire/explosion hazard"])
F("chemical_plant", "Chemicals complex", HI, "Chemicals", "Chemicals", "Basic and specialty chemicals manufacturing.", "Inputs for pharma, agriculture and manufacturing.", 3000, 36, land=150, power=40, water=10,
  cjobs=3000, jobs=J(u=600, s=800, e=300, r=50), inputs={"refined_fuels": 500000, "natural_gas": 100000}, out={"goods": [{"good": "chemicals", "qtyPerYear": 4000}], "valueAddedCrPerYear": 1200},
  req={"transport": ["road"], "tech": {"heavy_industry": 1}}, em={"pm25": 100, "nox": 800, "so2": 600, "voc": 1500, "co2": 500000}, tags=["chemicals", "manufacturing"], public=False)
F("textile_mill", "Textile & apparel park", HI, "Light manufacturing", "Textiles", "Spinning, weaving and garment factories.", "Labour-intensive manufacturing jobs and exports.", 1200, 24, land=200, power=40, water=8,
  cjobs=3000, jobs=J(u=15000, s=2000, e=100), out={"goods": [{"good": "textiles", "qtyPerYear": 2500}], "valueAddedCrPerYear": 1100}, req={"transport": ["road"]}, em={"pm25": 50, "co2": 150000, "voc": 100},
  tags=["textiles", "manufacturing", "labour_intensive"], public=False, basis="PM MITRA park scale")
F("auto_plant", "Automobile assembly plant (300,000 vehicles/yr)", HI, "Automotive", "Automotive", "Car and light-vehicle assembly plant.", "Large manufacturing employer with deep supplier networks.", 6000, 30, land=300, power=40, water=2,
  cjobs=4000, jobs=J(u=3000, s=4000, e=700, r=50), inputs={"steel": 300000, "electronic_components": 1500}, out={"goods": [{"good": "automobiles", "qtyPerYear": 300000}], "valueAddedCrPerYear": 4000},
  req={"transport": ["road", "rail"], "tech": {"automotive_ev": 1}, "supporting": [{"tag": "industrial_park", "count": 1, "radiusKm": 200, "why": "Auto plants rely on clustered component suppliers"}]}, em={"voc": 600, "pm25": 30, "co2": 150000},
  tags=["automotive", "manufacturing"], public=False)

# ============================ SEMICONDUCTORS ============================
SC = "Semiconductors"
F("semi_research", "Semiconductor research centre", SC, "Research", "Semiconductors", "Device physics, process R&D and design-enablement centre with a pilot line.", "Builds national semiconductor know-how and trains process engineers.", 1000, 30, land=20, power=15, water=1.5, opex=120,
  cjobs=800, jobs=J(u=50, s=150, e=250, r=250), out={"researchPoints": {"semiconductor": 60, "electronics": 10}}, tags=["semiconductor_research", "research"], tech=["process R&D", "pilot line"],
  req={"minGridReliability": 0.9, "supporting": [{"tag": "engineering_college", "count": 1, "radiusKm": 0, "why": "Research staff are drawn from engineering graduates"}]}, basis="Indicative; SCL Mohali modernisation / IMEC-like centres")
F("electronic_components", "Electronic component manufacturing", SC, "Components", "Electronics", "Passive components, PCBs and connectors.", "Domestic supply base for all electronics assembly.", 1000, 24, land=25, power=15, water=2,
  cjobs=1000, jobs=J(u=1500, s=800, e=200, r=20), inputs={"chemicals": 100}, out={"goods": [{"good": "electronic_components", "qtyPerYear": 1500}], "valueAddedCrPerYear": 450},
  req={"transport": ["road"], "tech": {"electronics": 1}}, tags=["electronics_components", "electronics", "manufacturing"], public=False, basis="ECMS-scheme scale component units")
F("specialty_gases", "Specialty chemicals & gases for electronics", SC, "Materials", "Chemicals", "Ultra-high-purity gases, wet chemicals and photoresists.", "Critical consumables for fabs and display plants.", 1500, 30, land=30, power=20, water=3,
  cjobs=1000, jobs=J(u=200, s=300, e=150, r=50), inputs={"chemicals": 400}, out={"goods": [{"good": "specialty_chemicals", "qtyPerYear": 20000}], "valueAddedCrPerYear": 400},
  req={"tech": {"materials": 2}}, em={"voc": 200, "co2": 50000}, tags=["specialty_chemicals", "manufacturing"], public=False, risks=["Hazardous materials handling"])
F("polysilicon_plant", "Polysilicon plant (10,000 t/yr)", SC, "Materials", "Semiconductors", "Siemens-process polysilicon production.", "Feedstock for solar wafers and (with further purification) semiconductor wafers.", 4000, 36, land=60, power=120, water=5,
  cjobs=2000, jobs=J(u=300, s=400, e=200, r=30), inputs={"quartz": 30000}, out={"goods": [{"good": "polysilicon", "qtyPerYear": 10000}], "valueAddedCrPerYear": 400},
  req={"tech": {"materials": 2}, "minGridReliability": 0.9}, em={"co2": 600000, "nox": 100}, tags=["polysilicon", "manufacturing"], public=False, risks=["Very electricity-intensive", "Global oversupply"])
F("wafer_plant", "Silicon wafer manufacturing (300 mm)", SC, "Materials", "Semiconductors", "Crystal growth, slicing and polishing of prime silicon wafers.", "Supplies wafers to fabs.", 8000, 36, land=40, power=60, water=6,
  cjobs=2000, jobs=J(u=200, s=600, e=400, r=80), inputs={"polysilicon": 2000}, out={"goods": [{"good": "silicon_wafer", "qtyPerYear": 1200000}], "valueAddedCrPerYear": 600},
  req={"tech": {"semiconductor": 2, "materials": 2}, "minGridReliability": 0.95}, tags=["silicon_wafer", "semiconductor_supply", "manufacturing"], public=False)
F("osat_plant", "Chip packaging, assembly & test (OSAT/ATMP)", SC, "Back-end", "Semiconductors", "Assembly, packaging and testing of processed wafers into finished chips.", "Usually the first step of a national semiconductor industry.", 7500, 30, land=40, power=40, water=4,
  cjobs=3000, jobs=J(u=1000, s=2000, e=800, r=50), inputs={"wafer_mature": 120000, "specialty_chemicals": 2000}, out={"goods": [{"good": "packaged_chips", "qtyPerYear": 3000}], "valueAddedCrPerYear": 1500},
  req={"transport": ["road", "airport"], "tech": {"semiconductor": 1, "electronics": 1}, "minGridReliability": 0.95, "supporting": [{"tag": "engineering_college", "count": 1, "radiusKm": 0, "why": "Packaging lines need process and test engineers"}]},
  tags=["chip_packaging", "semiconductor", "manufacturing"], public=False, tech=["wire bond", "flip chip"], basis="CG Power/Renesas OSAT ≈₹7,600 crore; Micron ATMP ≈₹22,500 crore (incl. phases)")
F("fab_mature", "Semiconductor fab, mature node (28-110 nm)", SC, "Front-end", "Semiconductors", "300 mm wafer fab (~40,000 wafers/month) for microcontrollers, power, automotive and analogue chips.", "Domestic production of mature-node chips used in vehicles, appliances and industry.", 91000, 60, land=120, power=150, water=30,
  cjobs=15000, jobs=J(u=800, s=2500, e=2200, r=300), inputs={"silicon_wafer": 480000, "specialty_chemicals": 8000, "semi_equipment": 40}, out={"goods": [{"good": "wafer_mature", "qtyPerYear": 480000}], "valueAddedCrPerYear": 9000},
  req={"transport": ["road", "airport"], "tech": {"semiconductor": 3, "materials": 2}, "minGridReliability": 0.99,
       "supporting": [{"tag": "semiconductor_research", "count": 1, "radiusKm": 0, "why": "Fabs need process-development capability and trained process engineers"},
                      {"tag": "specialty_chemicals", "count": 1, "radiusKm": 0, "why": "Fabs consume ultra-pure chemicals and gases continuously"},
                      {"tag": "engineering_college", "count": 2, "radiusKm": 0, "why": "A fab employs thousands of engineers"}]},
  em={"voc": 100, "nox": 50, "co2": 600000}, tags=["semiconductor_fab", "semiconductor", "manufacturing"], public=False, tech=["CMOS 28-110 nm", "DUV lithography"],
  risks=["Extremely capital-intensive; needs sustained subsidies", "Requires ~24/7 ultra-reliable power and ultra-pure water", "Yield ramp can take years", "Equipment import dependence"],
  basis="Tata-PSMC Dholera ≈₹91,000 crore for ~50,000 wspm", conf="medium")
F("fab_advanced", "Semiconductor fab, leading edge (≤7 nm)", SC, "Front-end", "Semiconductors", "Leading-edge logic fab using EUV lithography (~20,000 wafers/month).", "Manufacturing CPUs, GPUs and AI accelerators.", 160000, 72, land=150, power=300, water=40,
  cjobs=20000, jobs=J(u=800, s=3000, e=3500, r=800), inputs={"silicon_wafer": 240000, "specialty_chemicals": 10000, "semi_equipment": 60, "euv_lithography": 2}, out={"goods": [{"good": "wafer_advanced", "qtyPerYear": 240000}], "valueAddedCrPerYear": 25000},
  req={"transport": ["road", "airport"], "tech": {"semiconductor": 5, "materials": 3}, "minGridReliability": 0.995,
       "supporting": [{"tag": "semiconductor_fab", "count": 1, "radiusKm": 0, "why": "Leading-edge capability is built on experience operating mature-node fabs"},
                      {"tag": "semiconductor_research", "count": 2, "radiusKm": 0, "why": "Advanced process development needs deep R&D"},
                      {"tag": "research_university", "count": 1, "radiusKm": 0, "why": "Device physics and materials science research base"}]},
  em={"voc": 150, "nox": 80, "co2": 1200000}, tags=["semiconductor_fab", "advanced_fab", "semiconductor", "manufacturing"], public=False, tech=["FinFET/GAA", "EUV lithography"],
  risks=["EUV tools are supplied by a single vendor and subject to export licensing", "Only a few firms worldwide operate such fabs", "Enormous cost; failure risk is high"],
  basis="Leading-edge fabs cost ≈US$20 billion")
F("semi_equipment", "Semiconductor equipment manufacturing", SC, "Equipment", "Semiconductors", "Deposition, etch, cleaning and metrology tools.", "Reduces dependence on foreign tool makers.", 2500, 36, land=30, power=20, water=1,
  cjobs=1500, jobs=J(u=200, s=800, e=600, r=150), inputs={"electronic_components": 200, "advanced_materials": 150, "steel": 2000}, out={"goods": [{"good": "semi_equipment", "qtyPerYear": 40}], "valueAddedCrPerYear": 1200},
  req={"tech": {"semiconductor": 3, "robotics_automation": 2, "materials": 2}, "supporting": [{"tag": "semiconductor_research", "count": 1, "radiusKm": 0, "why": "Tool makers co-develop with process R&D"}]},
  tags=["semi_equipment", "semiconductor_supply", "manufacturing"], public=False)
F("litho_equipment", "Photolithography equipment & optics manufacturing", SC, "Equipment", "Semiconductors", "Precision optics, light sources and stages for lithography (DUV class).", "Strategic capability at the heart of chipmaking.", 5000, 60, land=30, power=30, water=1,
  cjobs=1500, jobs=J(u=100, s=600, e=800, r=400), inputs={"advanced_materials": 300, "electronic_components": 300}, out={"goods": [{"good": "semi_equipment", "qtyPerYear": 10}], "researchPoints": {"semiconductor": 20}, "valueAddedCrPerYear": 1500},
  req={"tech": {"semiconductor": 4, "materials": 3, "robotics_automation": 3}, "minGridReliability": 0.99, "supporting": [{"tag": "research_university", "count": 1, "radiusKm": 0, "why": "Optics and precision engineering research base"}]},
  tags=["litho_equipment", "semiconductor_supply", "manufacturing"], public=False, risks=["Decades of accumulated know-how required; very few global suppliers"])
F("display_fab", "Display panel fab (Gen 8.6)", SC, "Displays", "Electronics", "LCD/AMOLED display panel fabrication.", "Displays for phones, laptops and TVs.", 50000, 48, land=150, power=150, water=20,
  cjobs=8000, jobs=J(u=1000, s=2500, e=1500, r=200), inputs={"specialty_chemicals": 5000, "advanced_materials": 800}, out={"goods": [{"good": "display_panel", "qtyPerYear": 30000000}], "valueAddedCrPerYear": 6000},
  req={"tech": {"electronics": 3, "materials": 3}, "minGridReliability": 0.99}, em={"voc": 300, "co2": 800000}, tags=["display", "electronics", "manufacturing"], public=False)

# ============================ COMPUTING & AI ============================
CA = "Computing & AI"
F("gpu_plant", "GPU manufacturing & advanced packaging plant", CA, "Processors", "Computing", "Integrates GPU dies with HBM memory using 2.5D advanced packaging, then tests and builds accelerator modules.", "Domestic GPU supply for AI and HPC.", 25000, 48, land=50, power=80, water=6,
  cjobs=5000, jobs=J(u=500, s=2000, e=2500, r=500), inputs={"wafer_advanced": 30000, "hbm_memory": 600000, "specialty_chemicals": 1500, "electronic_components": 500}, out={"goods": [{"good": "gpu", "qtyPerYear": 100000}], "valueAddedCrPerYear": 6000},
  req={"transport": ["airport"], "tech": {"semiconductor": 4, "computing_ai": 3, "electronics": 3}, "minGridReliability": 0.99,
       "supporting": [{"tag": "chip_packaging", "count": 1, "radiusKm": 0, "why": "Advanced packaging builds on domestic OSAT experience"},
                      {"tag": "semiconductor_research", "count": 1, "radiusKm": 0, "why": "GPU design and packaging need deep semiconductor expertise"},
                      {"tag": "electronics_components", "count": 1, "radiusKm": 0, "why": "Substrates and passives are needed for every module"}]},
  tags=["gpu", "processor", "semiconductor", "manufacturing"], public=False, tech=["GPU design", "2.5D packaging", "HBM integration"],
  risks=["Needs leading-edge wafers (domestic ≤7 nm fab or imports subject to allocation)", "HBM memory is supplied by very few firms", "GPU architecture IP is a decades-long effort"],
  basis="Indicative; advanced-packaging facilities cost several US$ billion")
F("cpu_plant", "CPU manufacturing plant", CA, "Processors", "Computing", "Packages and tests server and client CPUs from processed wafers.", "Domestic processor supply.", 15000, 42, land=40, power=50, water=4,
  cjobs=4000, jobs=J(u=500, s=1500, e=1800, r=300), inputs={"wafer_advanced": 15000, "electronic_components": 300}, out={"goods": [{"good": "cpu", "qtyPerYear": 400000}], "valueAddedCrPerYear": 3500},
  req={"transport": ["airport"], "tech": {"semiconductor": 4, "computing_ai": 2}, "minGridReliability": 0.99, "supporting": [{"tag": "chip_packaging", "count": 1, "radiusKm": 0, "why": "CPU packaging builds on OSAT capability"}]},
  tags=["cpu", "processor", "semiconductor", "manufacturing"], public=False, tech=["CPU design", "ISA licensing / RISC-V"], risks=["ISA/IP licensing", "Leading-edge wafer access"])
F("ai_accelerator_plant", "AI accelerator manufacturing plant", CA, "Processors", "Computing", "Custom AI accelerator (ASIC/NPU) packaging and module production.", "Specialised AI chips, potentially on mature or advanced nodes.", 20000, 48, land=40, power=60, water=5,
  cjobs=4000, jobs=J(u=400, s=1500, e=2200, r=600), inputs={"wafer_advanced": 12000, "wafer_mature": 20000, "hbm_memory": 300000, "electronic_components": 300}, out={"goods": [{"good": "ai_accelerator", "qtyPerYear": 80000}], "valueAddedCrPerYear": 4500},
  req={"transport": ["airport"], "tech": {"semiconductor": 4, "computing_ai": 3}, "minGridReliability": 0.99, "supporting": [{"tag": "ai_lab", "count": 1, "radiusKm": 0, "why": "Accelerator architecture is co-designed with AI researchers"}]},
  tags=["ai_accelerator", "processor", "semiconductor", "manufacturing"], public=False)
F("ai_server_factory", "AI server manufacturing factory", CA, "Servers", "Computing", "Builds 8-GPU AI servers and racks: boards, power, liquid cooling, integration and burn-in.", "Turns processors into deployable AI compute.", 2000, 24, land=20, power=25, water=1,
  cjobs=1500, jobs=J(u=1000, s=1500, e=600, r=50), inputs={"gpu": 40000, "cpu": 10000, "electronic_components": 400, "dram_memory": 300}, out={"goods": [{"good": "ai_server", "qtyPerYear": 5000}], "valueAddedCrPerYear": 1500},
  req={"transport": ["road", "airport"], "tech": {"electronics": 2, "computing_ai": 2}, "supporting": [{"tag": "electronics_components", "count": 1, "radiusKm": 0, "why": "Server boards need PCBs, power supplies and passives"}]},
  tags=["ai_server", "server", "electronics", "manufacturing"], public=False, tech=["system integration", "liquid cooling"],
  risks=["Depends on GPU supply (domestic or imported)", "Thin margins for pure assembly"])
F("server_assembly", "Server assembly facility", CA, "Servers", "Computing", "General-purpose server and storage assembly.", "Supplies data centres and enterprises.", 800, 18, land=10, power=10,
  cjobs=800, jobs=J(u=800, s=700, e=200), inputs={"cpu": 60000, "electronic_components": 300, "dram_memory": 200}, out={"goods": [{"good": "server", "qtyPerYear": 60000}], "valueAddedCrPerYear": 600},
  req={"transport": ["road"], "tech": {"electronics": 1}}, tags=["server", "electronics", "manufacturing"], public=False)
F("memory_plant", "Memory chip assembly & test plant", CA, "Memory", "Semiconductors", "DRAM/NAND assembly and test.", "Memory supply for servers and devices.", 20000, 36, land=40, power=50, water=6,
  cjobs=4000, jobs=J(u=800, s=1500, e=900, r=100), inputs={"wafer_mature": 60000, "specialty_chemicals": 1000}, out={"goods": [{"good": "dram_memory", "qtyPerYear": 4000}], "valueAddedCrPerYear": 2500},
  req={"tech": {"semiconductor": 3}, "minGridReliability": 0.99}, tags=["memory", "semiconductor", "manufacturing", "chip_packaging"], public=False, basis="Micron Sanand ATMP ≈₹22,500 crore")
def dc(id, name, mw, capex, months, desc, extra_req=None, tags=()):
    F(id, name, CA, "Data centres", "Digital infrastructure", desc, "Hosts cloud, enterprise and AI workloads; requires electricity, cooling, connectivity and security.", capex, months, land=mw * 0.5, power=round(mw * 1.4, 1), water=round(mw * 0.03, 2),
      cjobs=int(mw * 20), jobs=J(u=int(mw * 1), s=int(mw * 2), e=int(mw * 1.5)), inputs={"server": mw * 120, "ai_server": mw * 4}, out={"computeMW": mw, "valueAddedCrPerYear": round(mw * 25)},
      req=dict({"transport": ["fibre", "road"], "minGridReliability": 0.99}, **(extra_req or {})), tags=["data_centre"] + list(tags), public=False, tech=["cooling", "virtualisation"],
      risks=["Very high, continuous electricity demand", "Water use for cooling", "Requires fibre backbone and physical security", "Server supply (domestic or imported)"],
      basis="≈₹60-70 crore per MW IT load excl. servers; PUE ≈1.4", env="Emissions depend on the grid's generation mix.")
dc("data_centre", "Data centre (50 MW IT)", 50, 3000, 24, "Colocation data centre with redundant power and cooling.")
dc("hyperscale_dc", "Hyperscale data centre (300 MW IT)", 300, 18000, 36, "Hyperscale campus for cloud and AI training.", tags=["hyperscale"])
F("cloud_region", "Cloud computing region (3 availability zones)", CA, "Data centres", "Digital infrastructure", "Software, networking and operations layer that turns multiple data centres into a public cloud region.", "Sovereign/public cloud services.", 5000, 24, land=5, power=5,
  cjobs=500, jobs=J(s=500, e=1500, r=50), out={"valueAddedCrPerYear": 3000, "researchPoints": {"computing_ai": 10}}, req={"transport": ["fibre"], "tech": {"computing_ai": 2}, "supporting": [{"tag": "data_centre", "count": 3, "radiusKm": 500, "why": "A cloud region spans at least three data-centre availability zones"}]},
  tags=["cloud", "digital_services"], public=False)
F("ai_lab", "AI research laboratory", CA, "Research", "AI", "Machine-learning research lab with GPU cluster access.", "Frontier AI research, models and talent.", 500, 18, land=5, power=10, opex=150,
  cjobs=200, jobs=J(s=100, e=300, r=400), out={"researchPoints": {"computing_ai": 60}}, req={"transport": ["fibre"], "supporting": [{"tag": "data_centre", "count": 1, "radiusKm": 0, "why": "AI research needs large-scale compute"}, {"tag": "university", "count": 1, "radiusKm": 0, "why": "Talent pipeline"}]},
  tags=["ai_lab", "research"])
F("supercomputer", "National supercomputing centre", CA, "HPC", "Computing", "Petascale supercomputer for science and engineering.", "Weather, climate, materials, drug discovery and defence simulation.", 1000, 24, land=3, power=15, water=0.3, opex=80,
  cjobs=200, jobs=J(s=80, e=150, r=150), inputs={"server": 2000, "gpu": 2000}, out={"computeMW": 10, "researchPoints": {"computing_ai": 20, "materials": 10, "biotech_pharma": 10, "aerospace_space": 8, "nuclear": 8, "quantum": 4}},
  req={"transport": ["fibre"], "minGridReliability": 0.99}, tags=["supercomputer", "hpc", "research"], basis="National Supercomputing Mission scale")
F("hpc_facility", "High-performance computing facility", CA, "HPC", "Computing", "University/industry HPC cluster.", "Simulation and data-intensive research.", 300, 12, land=1, power=4, opex=30,
  cjobs=80, jobs=J(s=30, e=60, r=60), inputs={"server": 600}, out={"computeMW": 3, "researchPoints": {"computing_ai": 6, "materials": 4}}, req={"transport": ["fibre"]}, tags=["hpc", "research"])

# ============================ OTHER TECHNOLOGY INDUSTRIES ============================
TI = "Technology Manufacturing"
def tech_fac(id, name, sub, industry, desc, purpose, capex, months, land, power, water, jobs, inputs, good, qty, va, techreq, tags, risks=(), support=None, transport=("road",), em=None, rp=None, minrel=0, basis=""):
    out = {"goods": [{"good": good, "qtyPerYear": qty}], "valueAddedCrPerYear": va}
    if rp: out["researchPoints"] = rp
    req = {"transport": list(transport), "tech": techreq}
    if support: req["supporting"] = support
    if minrel: req["minGridReliability"] = minrel
    F(id, name, TI, sub, industry, desc, purpose, capex, months, land=land, power=power, water=water, cjobs=int(capex / 2), jobs=jobs, inputs=inputs, out=out, req=req,
      em=em or {"voc": 50, "co2": power * 4000}, tags=list(tags) + ["manufacturing"], public=False, risks=risks, basis=basis)
tech_fac("smartphone_plant", "Smartphone manufacturing plant", "Consumer electronics", "Electronics", "High-volume smartphone assembly (~50 million units/yr).", "Large-scale electronics jobs and exports.", 2500, 18, 60, 30, 2,
         J(u=25000, s=4000, e=800), {"electronic_components": 6000, "display_panel": 50000000, "packaged_chips": 4000}, "smartphone", 50000000, 4000, {"electronics": 1},
         ["smartphone", "electronics", "labour_intensive"], ["Low domestic value addition if components are imported", "Dependence on global brands"], basis="Large PLI-scheme assembly plants")
tech_fac("laptop_plant", "Laptop & PC manufacturing plant", "Consumer electronics", "Electronics", "Laptop and desktop assembly.", "Domestic IT hardware.", 1500, 18, 30, 15, 1,
         J(u=6000, s=2000, e=400), {"electronic_components": 2000, "display_panel": 5000000, "cpu": 0, "packaged_chips": 1500}, "laptop", 5000000, 1500, {"electronics": 1}, ["laptop", "electronics"])
tech_fac("battery_gigafactory", "Battery cell gigafactory (10 GWh)", "Batteries", "Energy storage", "Lithium-ion cell manufacturing.", "Cells for EVs and grid storage.", 8000, 36, 100, 80, 3,
         J(u=1500, s=2000, e=800, r=150), {"lithium": 6500, "advanced_materials": 600, "electronic_components": 200}, "battery_cell", 10000000, 2000, {"automotive_ev": 1, "materials": 2, "energy": 2},
         ["battery", "manufacturing_clean"], ["Cathode/anode material import dependence", "Fire and chemical hazards"], minrel=0.95, basis="ACC PLI ≈₹800-1,000 crore per GWh")
tech_fac("ev_plant", "Electric vehicle manufacturing plant", "Automotive", "Automotive", "EV assembly (~200,000 cars/yr).", "Clean mobility and automotive jobs.", 5000, 30, 250, 35, 2,
         J(u=2500, s=3000, e=800, r=100), {"battery_cell": 10000000, "steel": 200000, "electronic_components": 1500, "packaged_chips": 600}, "electric_vehicle", 200000, 3000, {"automotive_ev": 2},
         ["ev", "automotive"], ["Battery supply", "Charging infrastructure needed for demand"], transport=("road", "rail"))
tech_fac("robotics_factory", "Robotics factory", "Automation", "Robotics", "Industrial robot arms and controllers.", "Productivity for all manufacturing.", 1000, 24, 20, 10, 1,
         J(u=300, s=800, e=600, r=150), {"electronic_components": 300, "steel": 10000, "packaged_chips": 200}, "robots", 5000, 600, {"robotics_automation": 2, "electronics": 2},
         ["robotics", "automation"], rp={"robotics_automation": 15})
tech_fac("automation_equipment", "Industrial automation equipment plant", "Automation", "Robotics", "PLCs, drives, sensors and SCADA systems.", "Automation for industry and utilities.", 800, 18, 15, 8, 0.5,
         J(u=300, s=700, e=400, r=50), {"electronic_components": 400, "packaged_chips": 300}, "automation_systems", 1500, 500, {"robotics_automation": 1, "electronics": 2}, ["automation"])
tech_fac("drone_factory", "Drone manufacturing", "Aerospace", "Aerospace", "Civil and defence UAV manufacturing.", "Agriculture, logistics, surveillance.", 300, 12, 10, 3, 0.2,
         J(u=200, s=300, e=200, r=30), {"electronic_components": 200, "battery_cell": 200000, "advanced_materials": 50}, "drones", 20000, 250, {"aerospace_space": 1, "electronics": 1}, ["drones", "aerospace"])
tech_fac("aerospace_plant", "Aerospace manufacturing plant", "Aerospace", "Aerospace", "Aero-structures, engines components and aircraft assembly.", "Strategic and export-oriented aerospace industry.", 5000, 42, 200, 30, 2,
         J(u=800, s=2500, e=1500, r=200), {"aluminium": 20000, "advanced_materials": 400, "electronic_components": 300}, "aircraft_components", 2500, 2000, {"aerospace_space": 2, "materials": 2},
         ["aerospace"], ["Long certification cycles"], transport=("road", "airport"), support=[{"tag": "airport", "count": 1, "radiusKm": 100, "why": "Aircraft assembly needs runway access"}])
tech_fac("satellite_factory", "Satellite manufacturing facility", "Space", "Space", "Satellite integration and test (clean rooms, thermal-vacuum).", "Communications, navigation and Earth-observation satellites.", 1500, 30, 30, 10, 0.5,
         J(u=100, s=500, e=700, r=300), {"electronic_components": 200, "advanced_materials": 100, "packaged_chips": 100}, "satellites", 20, 900, {"aerospace_space": 2, "electronics": 2},
         ["satellite", "space"], rp={"aerospace_space": 20}, transport=("road", "airport"))
tech_fac("defence_electronics", "Defence electronics plant", "Defence", "Defence", "Radars, communications and electronic-warfare systems.", "Strategic autonomy in defence systems.", 2000, 30, 50, 12, 1,
         J(u=300, s=1200, e=1200, r=300), {"electronic_components": 500, "packaged_chips": 300}, "defence_electronics", 2000, 1200, {"electronics": 2, "telecom": 2}, ["defence", "electronics"], rp={"electronics": 10})
tech_fac("telecom_equipment", "Telecommunications equipment factory", "Telecom", "Telecom", "4G/5G radio, core-network and transmission equipment.", "Trusted domestic telecom supply.", 1500, 24, 25, 10, 1,
         J(u=500, s=1200, e=800, r=150), {"electronic_components": 600, "packaged_chips": 500}, "telecom_equipment", 2500, 1200, {"telecom": 2, "electronics": 2}, ["telecom", "electronics"], rp={"telecom": 15})
tech_fac("fibre_optic_plant", "Fibre-optic cable manufacturing", "Telecom", "Telecom", "Optical preform, fibre drawing and cable plant.", "Fibre for national broadband backbones.", 800, 18, 20, 15, 1,
         J(u=400, s=400, e=150, r=20), {"quartz": 2000, "chemicals": 50}, "optical_fibre", 20000000, 400, {"telecom": 1, "materials": 1}, ["fibre_manufacturing", "telecom"])
tech_fac("advanced_materials_plant", "Advanced materials plant", "Materials", "Materials", "Carbon fibre, specialty alloys, ceramics and battery materials.", "Inputs for aerospace, batteries and semiconductors.", 2000, 30, 50, 40, 3,
         J(u=400, s=700, e=400, r=150), {"chemicals": 400, "rare_earths": 500, "aluminium": 5000}, "advanced_materials", 2000, 800, {"materials": 2}, ["advanced_materials", "materials"], rp={"materials": 15},
         em={"voc": 300, "nox": 200, "co2": 250000})
tech_fac("solar_module_plant", "Solar cell & module factory (2 GW/yr)", "Clean-tech", "Clean energy", "Solar cell and module manufacturing.", "Domestic solar supply chain.", 1500, 18, 40, 30, 2,
         J(u=1500, s=800, e=200, r=20), {"polysilicon": 5000, "aluminium": 15000, "chemicals": 100}, "solar_module", 2000, 600, {"energy": 1, "electronics": 1}, ["solar_manufacturing", "manufacturing_clean"])
tech_fac("rare_earth_magnet", "Rare-earth permanent magnet plant", "Materials", "Materials", "NdFeB magnet manufacturing.", "Magnets for EV motors, wind turbines, electronics.", 1200, 24, 20, 20, 1,
         J(u=200, s=400, e=200, r=50), {"rare_earths": 1500, "steel": 3000}, "rare_earth_magnets", 5000, 400, {"materials": 2}, ["magnets", "materials"])
F("quantum_centre", "Quantum computing research centre", TI, "Research", "Quantum", "Superconducting/photonic qubit research with cryogenic labs.", "Long-term quantum computing, sensing and communication capability.", 1500, 36, land=10, power=10, opex=150,
  cjobs=300, jobs=J(s=100, e=150, r=300), out={"researchPoints": {"quantum": 50, "materials": 5}}, req={"transport": ["fibre"], "minGridReliability": 0.99, "supporting": [{"tag": "research_university", "count": 1, "radiusKm": 0, "why": "Quantum research needs physicists and engineers from research universities"}]},
  tags=["quantum", "research"], risks=["Very long time to practical applications", "Talent scarcity"], basis="National Quantum Mission (₹6,003 crore over 8 years) scale")
F("materials_research", "Advanced materials research institute", TI, "Research", "Materials", "Metallurgy, ceramics, polymers and nano-materials research.", "Foundation for semiconductors, aerospace and energy.", 600, 24, land=10, power=6, opex=70,
  cjobs=300, jobs=J(s=100, e=100, r=300), out={"researchPoints": {"materials": 45, "heavy_industry": 10}}, tags=["materials_research", "research"])
F("aerospace_research", "Aerospace & space research centre", TI, "Research", "Space", "Propulsion, avionics and space-systems research.", "Builds aerospace and space capability.", 1000, 30, land=100, power=8, opex=120,
  cjobs=500, jobs=J(s=200, e=300, r=400), out={"researchPoints": {"aerospace_space": 50}}, tags=["aerospace_research", "research"])
F("energy_research", "Energy technology research centre", TI, "Research", "Energy", "Batteries, grid, hydrogen and renewable energy research.", "Clean-energy capability.", 500, 24, land=20, power=5, opex=60,
  cjobs=300, jobs=J(s=100, e=150, r=250), out={"researchPoints": {"energy": 45, "automotive_ev": 10}}, tags=["energy_research", "research"])
F("nuclear_research", "Nuclear research centre", TI, "Research", "Nuclear", "Reactor physics, fuel cycle and nuclear safety research with research reactor.", "Nuclear power and medical isotope capability.", 2500, 60, land=300, power=20, water=10, opex=250,
  cjobs=1500, jobs=J(u=200, s=500, e=700, r=800), out={"researchPoints": {"nuclear": 60, "materials": 10}}, tags=["nuclear_research", "research"], risks=["Safety and safeguards obligations"])
F("electronics_design_centre", "Electronics & chip design centre", TI, "Research", "Electronics", "VLSI design, EDA and embedded systems centre.", "Design capability for chips and systems.", 300, 12, land=3, power=3, opex=60,
  cjobs=150, jobs=J(s=100, e=500, r=150), out={"researchPoints": {"electronics": 30, "semiconductor": 15, "computing_ai": 5}}, req={"transport": ["fibre"]}, tags=["chip_design", "research"])
F("telecom_research", "Telecom research centre", TI, "Research", "Telecom", "5G/6G, optical networking and satellite communications research.", "Telecom capability.", 400, 18, land=5, power=3, opex=50,
  cjobs=150, jobs=J(s=100, e=250, r=150), out={"researchPoints": {"telecom": 45}}, tags=["telecom_research", "research"])
F("robotics_research", "Robotics & automation research centre", TI, "Research", "Robotics", "Robotics, control systems and AI-for-manufacturing research.", "Automation capability.", 400, 18, land=5, power=3, opex=50,
  cjobs=150, jobs=J(s=80, e=250, r=150), out={"researchPoints": {"robotics_automation": 45, "computing_ai": 5}}, tags=["robotics_research", "research"])
F("auto_research", "Automotive & EV research centre", TI, "Research", "Automotive", "Powertrain, battery-systems and vehicle-safety research.", "EV and automotive capability.", 400, 18, land=30, power=4, opex=50,
  cjobs=200, jobs=J(s=100, e=250, r=120), out={"researchPoints": {"automotive_ev": 45, "energy": 5}}, tags=["auto_research", "research"])
F("industrial_research", "Industrial technology institute", TI, "Research", "Industry", "Process engineering, metallurgy and manufacturing systems research.", "Heavy-industry capability.", 400, 18, land=20, power=4, opex=50,
  cjobs=200, jobs=J(s=120, e=250, r=100), out={"researchPoints": {"heavy_industry": 45, "materials": 5}}, tags=["industrial_research", "research"])
F("fibre_backbone", "Fibre-optic backbone", "Digital Infrastructure", "Telecom networks", "Telecom", "Long-haul optical fibre cable along a corridor.", "High-capacity connectivity for data centres, offices and households.", 0.05, 6, land=0, opex=0.002,
  cjobs=5, geometry="line", network="fibre", tags=["fibre", "telecom_network"], req={"grid": False}, basis="≈₹3-5 lakh/km (BharatNet-scale)")
F("telecom_node", "Telecom exchange & tower cluster", "Digital Infrastructure", "Telecom networks", "Telecom", "Core exchange, internet exchange point and mobile tower cluster.", "Local broadband and mobile service; fibre node.", 200, 12, land=2, power=3,
  cjobs=150, jobs=J(s=100, e=40), out={"valueAddedCrPerYear": 120}, nodeOf=["fibre"], tags=["fibre_node", "telecom_network"])

# ---------------------------------------------------------------------------
GOODS = [
 # id, name, unit, unit value ₹ crore, importability, per-capita demand/yr, income elasticity, category, note
 ("coal", "Coal", "t", 0.0002, 1.0, 0, 0, "Raw materials", "≈₹2,000/t (Coal India realisation)"),
 ("iron_ore", "Iron ore", "t", 0.0004, 1.0, 0, 0, "Raw materials", "≈₹4,000/t"),
 ("limestone", "Limestone", "t", 0.00005, 1.0, 0, 0, "Raw materials", "≈₹500/t"),
 ("bauxite", "Bauxite", "t", 0.00015, 1.0, 0, 0, "Raw materials", "≈₹1,500/t"),
 ("natural_gas", "Natural gas (LNG)", "t", 0.005, 1.0, 0, 0, "Raw materials", "≈US$600/t LNG"),
 ("crude_oil", "Crude oil", "t", 0.0045, 1.0, 0, 0, "Raw materials", "≈US$75/bbl"),
 ("uranium", "Natural uranium", "t", 1.4, 0.8, 0, 0, "Raw materials", "Imports under safeguards agreements"),
 ("rare_earths", "Rare-earth oxides", "t", 0.05, 0.5, 0, 0, "Raw materials", "Global supply concentrated; export restrictions possible"),
 ("lithium", "Lithium carbonate equivalent", "t", 0.1, 0.8, 0, 0, "Raw materials", ""),
 ("quartz", "High-purity quartz", "t", 0.0005, 1.0, 0, 0, "Raw materials", ""),
 ("polysilicon", "Polysilicon", "t", 0.08, 1.0, 0, 0, "Intermediate", "≈US$10/kg"),
 ("silicon_wafer", "Silicon wafer (300 mm)", "wafer", 0.0008, 0.9, 0, 0, "Semiconductor supply chain", "≈US$100 per prime wafer"),
 ("specialty_chemicals", "Electronic-grade chemicals & gases", "t", 0.02, 0.9, 0, 0, "Semiconductor supply chain", ""),
 ("semi_equipment", "Semiconductor manufacturing tools", "tool", 60, 0.7, 0, 0, "Semiconductor supply chain", "Average ≈US$7M per tool; subject to export licensing"),
 ("euv_lithography", "EUV lithography system", "system", 1500, 0.3, 0, 0, "Semiconductor supply chain", "Single global supplier; allocation and export licensing constrain access (assumption)"),
 ("wafer_mature", "Processed wafer, mature node", "wafer", 0.017, 0.9, 0, 0, "Semiconductor supply chain", "≈US$2,000 per processed wafer"),
 ("wafer_advanced", "Processed wafer, leading edge", "wafer", 0.14, 0.4, 0, 0, "Semiconductor supply chain", "≈US$17,000; foundry capacity allocation constrained (assumption)"),
 ("packaged_chips", "Packaged chips (value)", "₹ crore", 1, 0.95, 0.000005, 1.0, "Electronics", "Value-denominated"),
 ("hbm_memory", "HBM memory stack", "stack", 0.004, 0.5, 0, 0, "Semiconductor supply chain", "Very few suppliers; tight allocation (assumption)"),
 ("dram_memory", "Memory chips (value)", "₹ crore", 1, 0.95, 0.000002, 1.0, "Electronics", "Value-denominated"),
 ("gpu", "Data-centre GPU", "unit", 0.2, 0.5, 0, 0, "Computing", "≈US$25,000; export controls on advanced AI chips affect availability (assumption)"),
 ("ai_accelerator", "AI accelerator", "unit", 0.12, 0.6, 0, 0, "Computing", ""),
 ("cpu", "Server CPU", "unit", 0.04, 0.9, 0, 0, "Computing", "≈US$5,000"),
 ("ai_server", "AI server (8-GPU)", "unit", 2.5, 0.6, 0, 0, "Computing", "≈US$300,000"),
 ("server", "General-purpose server", "unit", 0.08, 1.0, 0.00002, 1.2, "Computing", "≈US$10,000"),
 ("electronic_components", "Electronic components & PCBs (value)", "₹ crore", 1, 1.0, 0.000004, 1.0, "Electronics", "Value-denominated"),
 ("display_panel", "Display panel", "unit", 0.0005, 1.0, 0, 0, "Electronics", "≈₹5,000"),
 ("smartphone", "Smartphone", "unit", 0.0015, 1.0, 0.107, 0.5, "Electronics", "Indian market ≈150 million units/yr"),
 ("laptop", "Laptop / PC", "unit", 0.005, 1.0, 0.01, 1.2, "Electronics", ""),
 ("battery_cell", "Li-ion battery cells", "kWh", 0.0008, 0.9, 0.005, 1.5, "Clean energy", "≈US$100/kWh"),
 ("electric_vehicle", "Electric car", "unit", 0.12, 1.0, 0.0002, 1.5, "Automotive", ""),
 ("automobiles", "Automobile (ICE)", "unit", 0.08, 1.0, 0.003, 1.2, "Automotive", "Indian market ≈4 million passenger vehicles/yr"),
 ("steel", "Steel", "t", 0.0055, 1.0, 0.1, 1.0, "Materials", "≈100 kg per capita consumption"),
 ("cement", "Cement", "t", 0.0005, 1.0, 0.29, 0.8, "Materials", "≈290 kg per capita"),
 ("aluminium", "Aluminium", "t", 0.022, 1.0, 0.003, 1.2, "Materials", ""),
 ("fertilizer", "Fertiliser (urea eq.)", "t", 0.003, 1.0, 0.025, 0.2, "Agriculture", "Unsubsidised cost; India ≈35 Mt urea"),
 ("food_grain", "Food grains", "t", 0.0025, 0.9, 0, 0, "Agriculture", "Food demand handled by the food model"),
 ("processed_food", "Processed food (value)", "₹ crore", 1, 1.0, 0.00004, 1.0, "Agriculture", "Value-denominated"),
 ("milk", "Milk", "t", 0.004, 1.0, 0.16, 0.5, "Agriculture", "India ≈460 g/person/day availability"),
 ("fish", "Fish", "t", 0.015, 1.0, 0.007, 0.8, "Agriculture", ""),
 ("pharmaceuticals", "Pharmaceuticals (value)", "₹ crore", 1, 1.0, 0.00014, 1.0, "Health", "Domestic market ≈₹2 lakh crore"),
 ("api", "Active pharmaceutical ingredients (value)", "₹ crore", 1, 0.9, 0, 0, "Health", "Value-denominated"),
 ("medical_devices", "Medical devices (value)", "₹ crore", 1, 1.0, 0.00006, 1.2, "Health", "Value-denominated"),
 ("chemicals", "Industrial chemicals (value)", "₹ crore", 1, 1.0, 0.00005, 1.0, "Materials", "Value-denominated"),
 ("refined_fuels", "Refined petroleum fuels", "t", 0.007, 1.0, 0.17, 0.8, "Energy", "≈0.17 t/capita consumption"),
 ("green_hydrogen", "Green hydrogen", "t", 0.033, 0.0, 0, 0, "Clean energy", "Not traded internationally at scale"),
 ("solar_module", "Solar modules", "MW", 2.0, 1.0, 0, 0, "Clean energy", "≈₹2 crore/MW"),
 ("textiles", "Textiles & apparel (value)", "₹ crore", 1, 1.0, 0.0001, 0.8, "Consumer goods", "Value-denominated"),
 ("robots", "Industrial robots", "unit", 0.3, 1.0, 0, 0, "Machinery", ""),
 ("automation_systems", "Automation systems (value)", "₹ crore", 1, 1.0, 0, 0, "Machinery", ""),
 ("drones", "Drones", "unit", 0.05, 1.0, 0, 0, "Aerospace", ""),
 ("aircraft_components", "Aircraft components (value)", "₹ crore", 1, 1.0, 0, 0, "Aerospace", ""),
 ("satellites", "Satellites", "unit", 50, 1.0, 0, 0, "Aerospace", ""),
 ("defence_electronics", "Defence electronics (value)", "₹ crore", 1, 0.6, 0, 0, "Defence", ""),
 ("telecom_equipment", "Telecom equipment (value)", "₹ crore", 1, 1.0, 0.00002, 1.0, "Telecom", ""),
 ("optical_fibre", "Optical fibre", "fibre-km", 0.00002, 1.0, 0, 0, "Telecom", ""),
 ("advanced_materials", "Advanced materials (value)", "₹ crore", 1, 0.8, 0, 0, "Materials", ""),
 ("rare_earth_magnets", "Rare-earth magnets", "t", 0.08, 0.6, 0, 0, "Materials", ""),
]

def main():
    cats = collections.OrderedDict()
    for f in FAC.values():
        cats.setdefault(f["category"], []).append(f)
    for fn in os.listdir(OUT):
        if fn.startswith("facilities_"):
            os.remove(os.path.join(OUT, fn))
    for i, (c, fs) in enumerate(cats.items()):
        slug = c.lower().replace("&", "and").replace(" ", "_")
        with open(os.path.join(OUT, f"facilities_{i:02d}_{slug}.json"), "w") as fh:
            json.dump({"category": c, "note": "Indicative modelling assumptions; see provenance on each entry.", "facilities": fs}, fh, indent=1, ensure_ascii=False)
    goods = [dict(id=g[0], name=g[1], unit=g[2], unitValueCr=g[3], importability=g[4], perCapitaDemand=g[5], incomeElasticity=g[6], category=g[7], note=g[8], kind="assumption",
                  source="Indicative price/volume assumption") for g in GOODS]
    with open(os.path.join(OUT, "goods.json"), "w") as fh:
        json.dump({"goods": goods}, fh, indent=1, ensure_ascii=False)
    ids = {g[0] for g in GOODS}
    for f in FAC.values():
        for x in f["inputs"] + f["outputs"].get("goods", []):
            assert x["good"] in ids, (f["id"], x["good"])
    print(len(FAC), "facilities in", len(cats), "categories;", len(GOODS), "goods")

main()
