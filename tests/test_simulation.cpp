// Integration tests for the simulation engines.
#include "advisor/Advisor.h"
#include "advisor/Suitability.h"
#include "core/DataPaths.h"
#include "scenario/ProjectionRunner.h"
#include "scenario/ScenarioStore.h"
#include "sim/Simulation.h"
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

using namespace isim;

static SimulationResources &res()
{
    static SimulationResources r;
    static bool loaded = false;
    if (!loaded) { QString e; if (!r.load(dataDir(), &e)) qFatal("%s", qPrintable(e)); loaded = true; }
    return r;
}

static int districtNamed(const QString &n)
{
    for (const auto &d : res().geo.districts()) if (d.name == n) return d.index;
    return -1;
}

static void completeAll(Simulation &s)
{
    for (auto &f : s.world().facilities) { f.progress = 1; f.status = FacilityStatus::Operating; }
    s.refresh();
    auto ctx = s.context();
    ctx.net.dirty = true;
}

class TestSimulation : public QObject {
    Q_OBJECT
private slots:
    void initialisation()
    {
        Simulation s(res());
        s.newScenario({});
        const auto &w = s.world();
        const auto ref = res().reference.valueAt("SP.POP.TOTL", "IND", 2023);
        QVERIFY(ref.available);
        QVERIFY(qAbs(w.stats.population - ref.value) / ref.value < 1e-6);
        QVERIFY(w.facilities.isEmpty());              // empty development layer
        QCOMPARE(w.stats.generationCapacityMW, 0.0);
        QCOMPARE(w.stats.powerServedMW, 0.0);
        QVERIFY(w.stats.powerDemandMW > 10000);
        QVERIFY(w.stats.gdpCr > 0);
        QVERIFY(w.stats.unemploymentRate > 0.02 && w.stats.unemploymentRate < 0.1);
        QVERIFY(w.stats.lifeExpectancy > 60 && w.stats.lifeExpectancy < 80);
    }
    void monthlyStepsAreFinite()
    {
        Simulation s(res());
        s.newScenario({});
        s.runMonths(24);
        const auto &st = s.world().stats;
        for (double v : {st.population, st.gdpCr, st.lifeExpectancy, st.tfr, st.pm25PopWeighted, st.unemploymentRate})
            QVERIFY(std::isfinite(v));
        QCOMPARE(s.world().month, 24);
        QVERIFY(s.world().history.size() == 25);
        // population grows modestly (TFR ≈2, young structure)
        QVERIFY(st.population > s.world().history.first().v.value("population"));
        QVERIFY(st.population < s.world().history.first().v.value("population") * 1.05);
    }
    void powerRequiresGridConnection()
    {
        Simulation s(res());
        s.newScenario({});
        const int di = districtNamed("Jaipur");
        QVERIFY(di >= 0);
        const QPointF c = res().geo.districts()[di].centroid;
        // a solar farm alone does not serve consumers
        QVERIFY(s.place({"solar_farm", c, {}, {}}).ok);
        completeAll(s);
        QCOMPARE(s.world().districts[di].powerServedMW, 0.0);
        // add distribution network in the same district -> served
        QVERIFY(s.place({"distribution_network", c + QPointF(0.05, 0.05), {}, {}}).ok);
        completeAll(s);
        QVERIFY(s.world().districts[di].powerServedMW > 0);
        // a neighbouring district is NOT served without a transmission line
        const int other = districtNamed("Ajmer");
        QVERIFY(other >= 0);
        QVERIFY(s.place({"substation", res().geo.districts()[other].centroid, {}, {}}).ok);
        completeAll(s);
        QCOMPARE(s.world().districts[other].powerServedMW, 0.0);
        QVERIFY(s.place({"transmission_line", {}, {c, res().geo.districts()[other].centroid}, {}}).ok);
        completeAll(s);
        QVERIFY(s.world().districts[other].powerServedMW > 0);
    }
    void roadConnectivityAndBridges()
    {
        Simulation s(res());
        s.newScenario({});
        // Patna (south bank of the Ganga) to Hajipur (north bank): crosses the Ganga
        const QPointF a(85.14, 25.55), b(85.21, 25.75);
        auto r = s.place({"national_highway", {}, {a, b}, {}});
        QVERIFY2(r.ok, qPrintable(r.error));
        completeAll(s);
        const auto *road = s.world().facilityByUid(r.uid);
        QVERIFY(road->networkBroken);                   // no bridge yet
        auto crossings = res().geo.riverCrossings(a, b);
        QVERIFY(!crossings.isEmpty());
        for (const auto &c : crossings) QVERIFY(s.place({"bridge", c.second, {}, {}}).ok);
        completeAll(s);
        QVERIFY(!s.world().facilityByUid(r.uid)->networkBroken);
        // a factory near the road has road access; one far away does not
        const auto near = s.place({"textile_mill", QPointF(85.15, 25.58), {}, {}});
        const auto far = s.place({"textile_mill", QPointF(84.0, 25.0), {}, {}});
        completeAll(s);
        QVERIFY(s.world().facilityByUid(near.uid)->roadAccess);
        QVERIFY(!s.world().facilityByUid(far.uid)->roadAccess);
        QVERIFY(s.world().facilityByUid(far.uid)->transportScore < 1.0);
    }
    void gpuFactoryIsNotRecommended()
    {
        Simulation s(res());
        s.newScenario({});
        auto ctx = s.context();
        const auto *gpu = res().catalog.facility("gpu_plant");
        const auto rep = SuitabilityEngine::assess(ctx, *gpu, QPointF(77.6, 12.97)); // Bengaluru
        QCOMPARE(rep.status, SuitabilityStatus::NotRecommended);
        QVERIFY(rep.estimatedEfficiency < 0.1);
        bool tech = false; for (const auto &r : rep.reasons) if (r.contains("Technology")) tech = true;
        QVERIFY(tech);
        QStringList ids; for (const auto &p : rep.prerequisites) ids << p.facilityId;
        QVERIFY(ids.contains("semi_research"));            // semiconductor research is recommended
        QVERIFY(ids.contains("substation"));               // grid connection is recommended
        for (const auto &p : rep.prerequisites) QVERIFY(!p.why.isEmpty()); // every prerequisite is explained
        // hard constraints: seaport inland is not feasible
        const auto port = SuitabilityEngine::assess(ctx, *res().catalog.facility("seaport"), QPointF(77.2, 28.6));
        QCOMPARE(port.status, SuitabilityStatus::NotFeasible);
    }
    void factoryDoesNotProduceWithoutPrerequisites()
    {
        Simulation s(res());
        s.newScenario({});
        const auto r = s.place({"gpu_plant", QPointF(77.6, 12.97), {}, {}});
        QVERIFY(r.ok);
        completeAll(s);
        s.stepMonth();
        const auto *f = s.world().facilityByUid(r.uid);
        QVERIFY(f->isOperating());
        QVERIFY(f->efficiency < 0.01);
        QVERIFY(!f->limitingFactors.isEmpty());
        QCOMPARE(s.world().goodsSupply.value("gpu"), 0.0);
    }
    void constructionProgressAndBudget()
    {
        Simulation s(res());
        s.newScenario({});
        const double t0 = s.world().budget.treasuryCr;
        const auto r = s.place({"phc", QPointF(80.95, 26.85), {}, {}});
        QVERIFY(r.ok);
        const int months = res().catalog.facility("phc")->constructionMonths;
        s.runMonths(months + 1);
        QVERIFY(s.world().facilityByUid(r.uid)->isOperating());
        QVERIFY(s.world().budget.treasuryCr != t0);
    }
    void policyRampAndHumanRightsHarms()
    {
        Simulation s(res());
        s.newScenario({});
        s.enactPolicy("two_child_penalties", 1.0);
        auto ctx = s.context();
        PolicyEngine pe;
        pe.computeModifiers(ctx);
        QCOMPARE(ctx.mods.mul("tfr"), 1.0); // nothing before the lag
        s.runMonths(60);
        auto ctx2 = s.context();
        pe.computeModifiers(ctx2);
        QVERIFY(ctx2.mods.mul("tfr") < 1.0);
        QVERIFY(ctx2.mods.add("sexratio.distortion") > 0);
        QVERIFY(ctx2.mods.mul("efficiency.mult.all") < 1.0);
        bool warned = false; for (const auto &n : s.world().log) if (n.text.contains("Human-rights")) warned = true;
        QVERIFY(warned);
    }
    void advisorUsesSimulationData()
    {
        Simulation s(res());
        s.newScenario({});
        s.runMonths(1);
        auto ctx = s.context();
        const auto recs = Advisor::analyse(ctx, &res().reference);
        QVERIFY(!recs.isEmpty());
        bool power = false;
        for (const auto &r : recs) {
            QVERIFY(!r.why.isEmpty());
            QVERIFY(!r.opportunityCost.isEmpty());
            QVERIFY(!r.alternatives.isEmpty());
            QVERIFY(!r.risks.isEmpty());
            if (r.id == "power.generation") { power = true; QVERIFY(r.investmentCr > 0); QVERIFY(!r.evidence.isEmpty()); QVERIFY(r.targetDistrict >= 0); }
        }
        QVERIFY(power);
    }
    void environmentRespondsToEmissions()
    {
        Simulation s(res());
        s.newScenario({});
        const int di = districtNamed("Korba");
        QVERIFY(di >= 0);
        const double before = s.world().districts[di].so2;
        const QPointF c = res().geo.districts()[di].centroid;
        s.place({"coal_plant", c, {}, {}});
        s.place({"distribution_network", c, {}, {}});
        // coal arrives by rail: a line and a station next to the plant
        QVERIFY(s.place({"railway_track", {}, {c + QPointF(-0.2, 0.01), c + QPointF(0.2, 0.01)}, {}}).ok);
        QVERIFY(s.place({"railway_station", c + QPointF(0.02, 0.01), {}, {}}).ok);
        QVERIFY(s.place({"state_highway", {}, {c + QPointF(-0.2, -0.01), c + QPointF(0.2, -0.01)}, {}}).ok);
        completeAll(s);
        s.stepMonth();
        const auto *plant = [&]() { for (const auto &f : s.world().facilities) if (f.defId == "coal_plant") return &f; return (const PlacedFacility *)nullptr; }();
        QVERIFY2(plant->efficiency > 0.3, qPrintable(plant->limitingFactors.join("; ")));
        QVERIFY(s.world().districts[di].so2 > before + 1.0);
        QVERIFY(s.world().stats.co2Mt > 0);
    }
    void saveLoadRoundTrip()
    {
        Simulation s(res());
        s.newScenario({"Round trip", 2025, 123456, 7});
        s.place({"phc", QPointF(80.95, 26.85), {}, {}});
        s.enactPolicy("education_spending", 0.5);
        s.runMonths(3);
        QTemporaryDir dir;
        const QString path = dir.filePath("t.indiasim");
        QString e;
        QVERIFY2(ScenarioStore::save(s.world(), path, &e), qPrintable(e));
        WorldState w;
        QVERIFY2(ScenarioStore::load(path, w, &e), qPrintable(e));
        QCOMPARE(w.name, QString("Round trip"));
        QCOMPARE(w.month, 3);
        QCOMPARE(w.facilities.size(), 1);
        QCOMPARE(w.policies.size(), 1);
        QCOMPARE(w.history.size(), s.world().history.size());
        Simulation s2(res());
        s2.setWorld(w);
        QVERIFY(qAbs(s2.world().stats.population - s.world().stats.population) / s.world().stats.population < 1e-9);
        // derived state (labour force, GDP) is rebuilt identically after loading
        QVERIFY2(qAbs(s2.world().stats.gdpCr - s.world().stats.gdpCr) / s.world().stats.gdpCr < 0.02,
                 qPrintable(QString("%1 vs %2").arg(s2.world().stats.gdpCr).arg(s.world().stats.gdpCr)));
        QHash<QString, QString> meta;
        QVERIFY(ScenarioStore::readMeta(path, meta));
        QCOMPARE(meta.value("schema_version"), QString::number(ScenarioStore::kSchemaVersion));
    }
    void projectionsHaveUncertaintyBands()
    {
        Simulation s(res());
        s.newScenario({});
        s.enactPolicy("family_planning", 1.0);
        const auto p = ProjectionRunner::run(s, 3, 4, 11);
        const auto *pop = p.find("population");
        QVERIFY(pop);
        QCOMPARE(pop->years.size(), 3);
        for (int i = 0; i < 3; ++i) QVERIFY(pop->p10[i] <= pop->p50[i] && pop->p50[i] <= pop->p90[i]);
        QVERIFY(!p.notes.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSimulation)
#include "test_simulation.moc"
