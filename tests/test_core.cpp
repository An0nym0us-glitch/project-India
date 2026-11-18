// Unit tests for data loading, GIS helpers, AQI and demographic calculations.
#include "catalog/Catalog.h"
#include "comparison/ReferenceData.h"
#include "core/DataPaths.h"
#include "core/ModelParameters.h"
#include "engines/Aqi.h"
#include "engines/PolicyEngine.h"
#include "engines/PopulationEngine.h"
#include "gis/GeoDatabase.h"
#include "gis/GeoMath.h"
#include <QtTest>

using namespace isim;

class TestCore : public QObject {
    Q_OBJECT
private slots:
    void parameters()
    {
        ModelParameters p;
        QString e;
        QVERIFY2(p.load(dataFile("model_parameters.json"), &e), qPrintable(e));
        QCOMPARE(p("water.lpcdUrban"), 135.0);
        QVERIFY(p.find("water.lpcdUrban")->provenance.kind == DataKind::Measured);
        // every parameter documents its basis
        for (const auto &k : p.keys()) {
            if (k.startsWith('_')) continue;
            QVERIFY2(!p.find(k)->description.isEmpty(), qPrintable(k));
            QVERIFY2(p.find(k)->low <= p.find(k)->value && p.find(k)->value <= p.find(k)->high, qPrintable(k));
        }
        std::mt19937_64 rng(1);
        auto s = p.sampled(rng);
        for (const auto &k : p.keys()) {
            const auto *o = p.find(k);
            QVERIFY(s(k) >= o->low - 1e-9 && s(k) <= o->high + 1e-9);
        }
    }
    void catalogue()
    {
        Catalog c;
        QString e;
        QVERIFY2(c.load(dataDir(), &e), qPrintable(e));
        QVERIFY(c.facilities().size() >= 150);
        // GPU factory, AI server factory and data centre are distinct facilities with distinct requirements
        const auto *gpu = c.facility("gpu_plant"), *srv = c.facility("ai_server_factory"), *dc = c.facility("data_centre");
        QVERIFY(gpu && srv && dc);
        QVERIFY(gpu->techRequired.value("semiconductor") >= 4);
        bool srvNeedsGpu = false; for (const auto &i : srv->inputs) if (i.good == "gpu") srvNeedsGpu = true;
        QVERIFY(srvNeedsGpu);
        QVERIFY(dc->transportNeeds.contains("fibre") && dc->minGridReliability >= 0.99);
        // every supporting tag referenced exists on some facility
        QSet<QString> tags;
        for (const auto &f : c.facilities()) for (const auto &t : f.tags) tags.insert(t);
        for (const auto &f : c.facilities()) for (const auto &s : f.supporting) QVERIFY2(tags.contains(s.tag), qPrintable(f.id + ":" + s.tag));
        for (const auto &d : c.techDomains()) for (const auto &l : d.levels) for (const auto &t : l.requiredFacilityTags) QVERIFY2(tags.contains(t), qPrintable(d.id + ":" + t));
        // search by name, technology, resource
        QVERIFY(!c.search("gpu").isEmpty());
        QCOMPARE(c.search("gpu").first()->id, QString("gpu_plant"));
        QVERIFY(!c.search("lithography", SearchField::Name).isEmpty());
        bool coalUser = false; for (const auto *f : c.search("coal", SearchField::Resource)) if (f->id == "steel_plant") coalUser = true;
        QVERIFY(coalUser);
        QVERIFY(!c.search("semiconductor", SearchField::Technology).isEmpty());
    }
    void geography()
    {
        GeoDatabase g;
        QString e;
        QVERIFY2(g.load(dataFile("geo"), &e), qPrintable(e));
        QCOMPARE(g.states().size(), 36);
        QCOMPARE(g.districts().size(), 640);
        auto stateName = [&](double lon, double lat) { const int s = g.stateAt({lon, lat}); return s >= 0 ? g.states()[s].name : QString(); };
        QCOMPARE(stateName(77.21, 28.61), QString("Delhi"));
        QCOMPARE(stateName(72.88, 19.08), QString("Maharashtra"));     // Mumbai
        QCOMPARE(stateName(77.59, 12.97), QString("Karnataka"));       // Bengaluru
        QCOMPARE(stateName(88.36, 22.57), QString("West Bengal"));     // Kolkata
        QCOMPARE(g.districtAt({80.0, 15.0}) >= 0, true);
        QCOMPARE(g.districtAt({70.0, 15.0}), -1);                        // Arabian Sea
        QVERIFY(g.distanceToCoastKm({72.88, 19.08}) < 10);
        QVERIFY(g.distanceToCoastKm({77.21, 28.61}) > 700);
        QVERIFY(g.isMountainous({77.17, 31.10}));                         // Shimla
        QVERIFY(!g.search("Pune").isEmpty());
        double census = 0; for (const auto &d : g.districts()) census += d.census.population;
        QCOMPARE(qint64(census), qint64(1210854977));                     // Census 2011 total
    }
    void geoMath()
    {
        QVERIFY(qAbs(geo::haversineKm({77.21, 28.61}, {72.88, 19.08}) - 1150) < 20); // Delhi-Mumbai ≈1,150 km
        QVERIFY(geo::segmentsIntersect({0, 0}, {1, 1}, {0, 1}, {1, 0}));
        QVERIFY(!geo::segmentsIntersect({0, 0}, {1, 0}, {0, 1}, {1, 1}));
    }
    void aqi()
    {
        CpcbAqi a;
        QVERIFY(a.load(dataFile("aqi/cpcb_breakpoints.json")));
        QCOMPARE(a.subIndex("pm25", 30), 50);
        QCOMPARE(a.subIndex("pm25", 60), 100);
        QCOMPARE(a.subIndex("pm25", 90), 200);
        QCOMPARE(a.subIndex("pm25", 250), 400);
        QCOMPARE(a.subIndex("pm10", 100), 100);
        QCOMPARE(a.subIndex("pm10", 250), 200);
        QCOMPARE(a.subIndex("no2", 180), 200);
        QCOMPARE(a.subIndex("co", 2.0), 100);
        QCOMPARE(a.subIndex("pm25", 1000), 500);
        auto r = a.compute({{"pm25", 95}, {"pm10", 120}, {"no2", 30}});
        QCOMPARE(r.dominant, QString("pm25"));
        QCOMPARE(r.category, QString("Poor"));
        // fewer than three pollutants: not computable (CPCB rule)
        QCOMPARE(a.compute({{"pm25", 95}, {"no2", 30}}).aqi, -1);
        // three pollutants without PM: not computable
        QCOMPARE(a.compute({{"no2", 30}, {"so2", 10}, {"co", 1}}).aqi, -1);
    }
    void lifeTable()
    {
        const double b = PopulationEngine::kGompertzB;
        for (double target : {55.0, 65.0, 72.0, 80.0}) {
            const double c = PopulationEngine::childMortality(target);
            const double a = PopulationEngine::solveMortalityLevel(target, b, c);
            QVERIFY(qAbs(PopulationEngine::lifeExpectancy(a, b, c) - target) < 0.05);
        }
    }
    void policies()
    {
        PolicyLibrary lib;
        QVERIFY(lib.load(dataFile("policies/policies.json")));
        QVERIFY(lib.all().size() >= 30);
        const auto *coercive = lib.find("two_child_penalties");
        QVERIFY(coercive && coercive->isCoercive());
        bool hasHarm = false; for (const auto &e : coercive->effects) if (e.key == "governance.trust" && e.full < 0) hasHarm = true;
        QVERIFY(hasHarm); // coercive policies are not modelled as consequence-free
        for (const auto &p : lib.all()) {
            QVERIFY2(!p.unintended.isEmpty() || p.id == "governance_reform", qPrintable(p.id));
            QVERIFY(p.effLow <= p.effCentral && p.effCentral <= p.effHigh);
        }
    }
    void referenceData()
    {
        ReferenceData r;
        QVERIFY(r.load(dataFile("reference/worldbank_reference.json")));
        const auto pop = r.valueAt("SP.POP.TOTL", "IND", 2023);
        QVERIFY(pop.available && pop.value > 1.3e9 && pop.value < 1.5e9);
        const auto none = r.valueAt("IS.RRD.TOTL.KM", "IND", 2023);
        QVERIFY(!none.available); // missing data must be reported as unavailable, not invented
        QVector<double> m, f; double ms;
        QVERIFY(r.indiaAgeShares(2023, m, f, ms));
        double tot = 0; for (double x : m) tot += x;
        QVERIFY(qAbs(tot - 100) < 1.0);
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "test_core.moc"
