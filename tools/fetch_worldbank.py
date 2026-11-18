#!/usr/bin/env python3
"""Fetch real-world reference indicators from the World Bank WDI API.

Output: data/reference/worldbank_reference.json
Each value carries the indicator code, World Bank name, unit, source,
the actual year of the observation and the retrieval date. Missing
observations are stored as null and displayed as "unavailable" in the app.
Run:  python3 tools/fetch_worldbank.py
"""
import json, sys, time, datetime, urllib.request, urllib.error

COUNTRIES = ["IND", "CHN", "USA", "JPN", "RUS"]
FIRST_YEAR, LAST_YEAR = 1990, 2024
# (code, group, short label, unit shown in app, higher_is_better or None)
INDICATORS = [
    ("NY.GDP.MKTP.CD", "Economy", "GDP (nominal)", "current US$", True),
    ("NY.GDP.MKTP.PP.CD", "Economy", "GDP (PPP)", "current international $", True),
    ("NY.GDP.PCAP.CD", "Economy", "GDP per capita (nominal)", "current US$", True),
    ("NY.GDP.PCAP.PP.CD", "Economy", "GDP per capita (PPP)", "current international $", True),
    ("NY.GDP.MKTP.KD.ZG", "Economy", "GDP growth", "annual %", True),
    ("NV.IND.TOTL.ZS", "Economy", "Industry share of GDP", "% of GDP", None),
    ("NV.IND.MANF.ZS", "Economy", "Manufacturing share of GDP", "% of GDP", None),
    ("NE.EXP.GNFS.CD", "Economy", "Exports of goods & services", "current US$", True),
    ("NE.IMP.GNFS.CD", "Economy", "Imports of goods & services", "current US$", None),
    ("GC.DOD.TOTL.GD.ZS", "Economy", "Central government debt", "% of GDP", False),
    ("SL.GDP.PCAP.EM.KD", "Economy", "GDP per person employed", "constant 2021 PPP $", True),
    ("SP.POP.TOTL", "Population", "Total population", "persons", None),
    ("SP.POP.GROW", "Population", "Population growth", "annual %", None),
    ("SP.POP.1564.TO.ZS", "Population", "Working-age population (15-64)", "% of total", None),
    ("SP.URB.TOTL.IN.ZS", "Population", "Urban population", "% of total", None),
    ("SL.TLF.TOTL.IN", "Population", "Labour force", "persons", None),
    ("SP.POP.DPND", "Population", "Age dependency ratio", "% of working-age pop.", False),
    ("SP.DYN.LE00.IN", "Population", "Life expectancy at birth", "years", True),
    ("SP.DYN.TFRT.IN", "Population", "Fertility rate", "births per woman", None),
    ("SL.UEM.TOTL.ZS", "Population", "Unemployment (ILO modelled)", "% of labour force", False),
    ("SL.TLF.CACT.ZS", "Population", "Labour force participation (15+)", "% of pop. 15+", None),
    ("EG.ELC.ACCS.ZS", "Infrastructure", "Access to electricity", "% of population", True),
    ("EG.USE.ELEC.KH.PC", "Infrastructure", "Electric power consumption per capita", "kWh per capita", True),
    ("EG.ELC.RNEW.ZS", "Infrastructure", "Renewable electricity output share", "% of total electricity output", True),
    ("IS.RRD.TOTL.KM", "Infrastructure", "Rail lines", "route-km", True),
    ("IS.SHP.GOOD.TU", "Infrastructure", "Container port traffic", "TEU", True),
    ("IS.AIR.DPRT", "Infrastructure", "Air transport departures", "registered carrier departures", True),
    ("IT.NET.USER.ZS", "Infrastructure", "Internet users", "% of population", True),
    ("LP.LPI.OVRL.XQ", "Infrastructure", "Logistics Performance Index", "score 1-5", True),
    ("GB.XPD.RSDV.GD.ZS", "Technology", "R&D expenditure", "% of GDP", True),
    ("IP.PAT.RESD", "Technology", "Patent applications (residents)", "applications", True),
    ("SP.POP.SCIE.RD.P6", "Technology", "Researchers in R&D", "per million people", True),
    ("TX.VAL.TECH.MF.ZS", "Technology", "High-technology exports", "% of manufactured exports", True),
    ("TX.VAL.TECH.CD", "Technology", "High-technology exports", "current US$", True),
    ("SE.ADT.LITR.ZS", "Human development", "Adult literacy rate", "% of people 15+", True),
    ("SE.PRM.ENRR", "Human development", "Primary school enrolment", "% gross", None),
    ("SE.SEC.ENRR", "Human development", "Secondary school enrolment", "% gross", True),
    ("SE.TER.ENRR", "Human development", "Tertiary enrolment", "% gross", True),
    ("SH.XPD.CHEX.GD.ZS", "Human development", "Current health expenditure", "% of GDP", None),
    ("SH.MED.PHYS.ZS", "Human development", "Physicians", "per 1,000 people", True),
    ("SH.MED.BEDS.ZS", "Human development", "Hospital beds", "per 1,000 people", True),
    ("SH.STA.BASS.ZS", "Human development", "Basic sanitation services", "% of population", True),
    ("EN.ATM.PM25.MC.M3", "Environment", "PM2.5 mean annual exposure", "micrograms per cubic metre", False),
    ("EN.GHG.CO2.MT.CE.AR5", "Environment", "CO2 emissions (total, excl. LULUCF)", "Mt CO2e", False),
    ("EN.GHG.CO2.PC.CE.AR5", "Environment", "CO2 emissions per capita (excl. LULUCF)", "t CO2e per capita", False),
    ("EG.FEC.RNEW.ZS", "Environment", "Renewable energy consumption", "% of final energy consumption", True),
    ("AG.LND.FRST.ZS", "Environment", "Forest area", "% of land area", True),
    ("ER.H2O.FWST.ZS", "Environment", "Freshwater withdrawal (water stress)", "% of available freshwater", False),
]
# Population age structure for India (5-year groups, % of sex population).
AGE_GROUPS = ["0004","0509","1014","1519","2024","2529","3034","3539","4044","4549",
              "5054","5559","6064","6569","7074","7579","80UP"]

import csv, io, os, zipfile
CACHE = os.environ.get("WB_CACHE", os.path.join(os.path.dirname(__file__), ".wb_cache"))

def download(code, tries=6):
    """Bulk CSV download (more reliable than the JSON API); cached on disk."""
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, code + ".zip")
    if os.path.exists(path) and os.path.getsize(path) > 1000:
        return path
    url = f"https://api.worldbank.org/v2/en/indicator/{code}?downloadformat=csv"
    for i in range(tries):
        try:
            with urllib.request.urlopen(url, timeout=120) as r:
                data = r.read()
            zipfile.ZipFile(io.BytesIO(data))  # validate
            with open(path, "wb") as f:
                f.write(data)
            return path
        except Exception as e:
            print("  retry", i + 1, code, e, file=sys.stderr)
            time.sleep(3 * (i + 1))
    raise RuntimeError("failed " + code)

def series(code, countries):
    try:
        z = zipfile.ZipFile(download(code, tries=3))
    except Exception as e:  # indicator archived/removed: record as unavailable
        print("  UNAVAILABLE", code, e, file=sys.stderr)
        return {c: {} for c in countries}, {"unavailable": "Not available from the World Bank bulk download at retrieval time"}
    out = {c: {} for c in countries}
    meta = {}
    main = [n for n in z.namelist() if n.startswith("API_")][0]
    text = z.read(main).decode("utf-8-sig")
    rows = list(csv.reader(io.StringIO(text)))
    hdr_i = next(i for i, r in enumerate(rows) if r and r[0] == "Country Name")
    hdr = rows[hdr_i]
    for r in rows[hdr_i + 1:]:
        if len(r) < 4:
            continue
        meta["name"] = r[2]
        if r[1] in out:
            for j, y in enumerate(hdr):
                if y.isdigit() and FIRST_YEAR <= int(y) <= LAST_YEAR and j < len(r) and r[j].strip():
                    out[r[1]][y] = float(r[j])
    for n in z.namelist():
        if n.startswith("Metadata_Indicator"):
            mrows = list(csv.reader(io.StringIO(z.read(n).decode("utf-8-sig"))))
            if len(mrows) > 1 and len(mrows[1]) > 3:
                meta["sourceOrganization"] = mrows[1][3]
    return out, meta

def main():
    result = {"source": "World Bank, World Development Indicators (WDI) API v2",
              "sourceUrl": "https://data.worldbank.org/",
              "license": "CC BY 4.0",
              "retrieved": datetime.date.today().isoformat(),
              "countries": {"IND": "India", "CHN": "China", "USA": "United States", "JPN": "Japan", "RUS": "Russian Federation"},
              "indicators": []}
    for code, group, label, unit, hib in INDICATORS:
        print("fetch", code, file=sys.stderr)
        data, meta = series(code, COUNTRIES)
        result["indicators"].append({"code": code, "group": group, "label": label,
            "wbName": meta.get("name", label), "sourceOrganization": meta.get("sourceOrganization", ""), "availabilityNote": meta.get("unavailable", ""), "unit": unit, "higherIsBetter": hib, "series": data})
    ages = {}
    for sex in ("MA", "FE"):
        for g in AGE_GROUPS:
            code = f"SP.POP.{g}.{sex}.5Y" if g != "80UP" else f"SP.POP.80UP.{sex}.5Y"
            data, _ = series(code, ["IND"])
            ages[f"{sex}_{g}"] = data["IND"]
    ms, _ = series("SP.POP.TOTL.MA.ZS", ["IND"])
    result["indiaAgeStructure"] = {"description": "Population ages by 5-year group, % of male/female population (World Bank WDI SP.POP.xxxx.MA/FE.5Y)",
                                   "maleSharePct": ms["IND"], "groups": ages}
    with open("data/reference/worldbank_reference.json", "w") as f:
        json.dump(result, f, indent=1)
    print("ok", file=sys.stderr)

if __name__ == "__main__":
    main()
