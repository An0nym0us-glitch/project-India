#include <algorithm>
#include "InfoPanel.h"
#include "Format.h"
#include "ai/AIContextBuilder.h"
#include "catalog/Catalog.h"
#include "engines/Aqi.h"
#include "engines/ConstructionEngine.h"
#include "engines/EconomyEngine.h"
#include "gis/GeoDatabase.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QScrollBar>
#include <QVBoxLayout>

namespace isim {
using namespace fmt;

InfoPanel::InfoPanel(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    m_view = new QTextBrowser;
    m_view->setOpenExternalLinks(true);
    lay->addWidget(m_view, 1);
    auto *row = new QHBoxLayout;
    m_place = new QPushButton(tr("Place on map"));
    m_assess = new QPushButton(tr("Assess at selected district"));
    m_ai = new QPushButton(tr("AI analysis"));
    m_demolish = new QPushButton(tr("Demolish"));
    row->addWidget(m_place); row->addWidget(m_assess); row->addWidget(m_ai); row->addWidget(m_demolish);
    lay->addLayout(row);
    connect(m_place, &QPushButton::clicked, this, [this]() { if (m_app->selection().type == Selection::Definition) m_app->beginPlacement(m_app->selection().defId); });
    connect(m_demolish, &QPushButton::clicked, this, [this]() {
        if (m_app->selection().type == Selection::Facility &&
            QMessageBox::question(this, tr("Demolish"), tr("Remove this facility? Capital already spent is not recovered.")) == QMessageBox::Yes)
            m_app->demolish(m_app->selection().index);
    });
    connect(m_assess, &QPushButton::clicked, this, [this]() {
        // assess the selected definition at the most recently selected district (or the largest district)
        const auto *d = m_app->resources().catalog.facility(m_app->selection().defId);
        if (!d) return;
        int di = property("lastDistrict").toInt();
        if (di < 0 || di >= m_app->resources().geo.districts().size()) di = 0;
        auto ctx = m_app->sim().context();
        showPreview(SuitabilityEngine::assess(ctx, *d, m_app->resources().geo.districts()[di].centroid), d->id);
    });
    connect(m_ai, &QPushButton::clicked, this, [this]() {
        const auto &sel = m_app->selection();
        if (sel.type == Selection::Definition || sel.type == Selection::Facility) {
            QString defId = sel.defId;
            if (sel.type == Selection::Facility) if (const auto *f = m_app->sim().world().facilityByUid(sel.index)) defId = f->defId;
            const auto *d = m_app->resources().catalog.facility(defId);
            if (!d) return;
            emit askAI("explain_facility", AIContextBuilder::facility(m_app->sim(), *d, m_hasRep && m_repDef == defId ? &m_rep : nullptr), d->name);
        } else emit askAI("interpret_report", AIContextBuilder::nationalSnapshot(m_app->sim()), tr("National report"));
    });
    setProperty("lastDistrict", -1);
    connect(app, &AppController::selectionChanged, this, [this]() {
        if (m_app->selection().type == Selection::District) setProperty("lastDistrict", m_app->selection().index);
        refresh();
    });
    connect(app, &AppController::worldUpdated, this, [this]() { if (m_app->selection().type != Selection::Definition) refresh(); });
    refresh();
}

void InfoPanel::showPreview(const SuitabilityReport &rep, const QString &defId)
{
    m_rep = rep;
    m_repDef = defId;
    m_hasRep = true;
    if (rep.district >= 0) setProperty("lastDistrict", rep.district);
    if (m_app->selection().type == Selection::Definition && m_app->selection().defId == defId) refresh();
}

static QString row(const QString &k, const QString &v) { return QStringLiteral("<tr><td style='color:#888;padding-right:8px;white-space:nowrap' valign='top'>%1</td><td>%2</td></tr>").arg(k, v); }
static QString listHtml(const QStringList &l) { if (l.isEmpty()) return QStringLiteral("—"); QString s = "<ul style='margin:0;-qt-list-indent:1'>"; for (const auto &x : l) s += "<li>" + x.toHtmlEscaped() + "</li>"; return s + "</ul>"; }

QString InfoPanel::definitionHtml(const FacilityDefinition &d, const SuitabilityReport *rep) const
{
    const auto &cat = m_app->resources().catalog;
    auto ctx = m_app->sim().context();
    QString h;
    h += QStringLiteral("<h2 style='margin-bottom:0'>%1</h2><div style='color:%2'><b>%3</b> › %4 · %5</div>")
             .arg(esc(d.name), categoryColor(d.category).name(), esc(d.category), esc(d.subcategory), esc(d.industry));
    h += "<p>" + badge(d.provenance.kind) + QStringLiteral(" <small>confidence: %1 — %2</small></p>").arg(esc(d.provenance.confidence), esc(d.basis));
    if (rep) {
        const QString col = rep->status == SuitabilityStatus::Recommended ? "#2a9d4b" : rep->status == SuitabilityStatus::FeasibleWithGaps ? "#c98a00" : rep->status == SuitabilityStatus::NotRecommended ? "#d2601a" : "#c0392b";
        h += QStringLiteral("<div style='border:2px solid %1;padding:6px'><b>Suitability at %2</b><br><span style='color:%1;font-size:14pt'><b>%3</b></span><br>Expected output at current national conditions: <b>%4</b>")
                 .arg(col, esc(rep->location), esc(rep->statusText), pct(rep->estimatedEfficiency, 0));
        if (!rep->reasons.isEmpty()) h += "<br><b>Reasons:</b>" + listHtml(rep->reasons);
        h += "<table>";
        for (const auto &c : rep->checks) {
            const QString icon = c.level == CheckLevel::Ok ? "<span style='color:#2a9d4b'>✔</span>" : c.level == CheckLevel::Warning ? "<span style='color:#c98a00'>▲</span>" : c.level == CheckLevel::Problem ? "<span style='color:#d2601a'>✖</span>" : "<span style='color:#c0392b'>⛔</span>";
            h += QStringLiteral("<tr><td valign='top'>%1</td><td valign='top'><b>%2</b></td><td>%3</td></tr>").arg(icon, esc(c.area), esc(c.detail));
        }
        h += "</table>";
        if (!rep->prerequisites.isEmpty()) {
            h += "<b>Recommended prerequisite investments</b><ol>";
            for (const auto &p : rep->prerequisites) h += QStringLiteral("<li><b>%1</b> — %2</li>").arg(esc(p.title), esc(p.why));
            h += "</ol>";
        }
        h += QStringLiteral("<small>%1</small></div>").arg(esc(rep->regionalSuitability));
    } else h += "<p><i>Hover over the map in placement mode (or press “Assess…”) for a site-specific suitability assessment.</i></p>";

    h += "<table cellspacing='2'>";
    int n = 1;
    auto R = [&](const QString &k, const QString &v) { h += row(QStringLiteral("%1. %2").arg(n++).arg(k), v); };
    R(tr("Description"), esc(d.description));
    R(tr("Purpose & importance"), esc(d.purpose));
    const double capex = ConstructionEngine::effectiveCapex(ctx, d, 1.0);
    R(tr("Construction cost"), cr(capex) + " " + d.unitLabel() + (std::abs(capex - d.capexCr) > 0.01 ? QStringLiteral(" (base %1; policy-adjusted)").arg(cr(d.capexCr)) : QString())
                                + QStringLiteral("<br><small>Government share at current policies: %1</small>").arg(pct(ConstructionEngine::publicShareFor(ctx, d), 0)));
    R(tr("Required land"), num(d.landHa, 1) + " ha " + d.unitLabel());
    R(tr("Electricity requirement"), d.powerDemandMW > 0 ? mw(d.powerDemandMW) + (d.minGridReliability > 0 ? QStringLiteral(" (needs ≥%1 reliability)").arg(pct(d.minGridReliability, 1)) : QString()) : tr("negligible"));
    R(tr("Water requirement"), d.waterDemandMLD > 0 ? num(d.waterDemandMLD, 2) + " MLD" : tr("negligible"));
    R(tr("Required workforce"), QStringLiteral("%1 operations jobs; %2 construction jobs").arg(num(d.operationsJobs.total()), num(d.constructionJobs)));
    QStringList skills;
    for (int k = 0; k < SkillCount; ++k) if (d.operationsJobs[k] > 0) skills << QStringLiteral("%1: %2").arg(skillName(k), num(d.operationsJobs[k]));
    R(tr("Required skills"), skills.isEmpty() ? "—" : skills.join("<br>"));
    QStringList inputs;
    for (const auto &i : d.inputs) if (i.qtyPerYear > 0) { const auto *g = cat.good(i.good); inputs << QStringLiteral("%1 %2/yr of %3 (import availability %4)").arg(num(i.qtyPerYear), g ? g->unit : "", g ? g->name : i.good, g ? pct(g->importability, 0) : "?"); }
    R(tr("Raw materials & components"), inputs.isEmpty() ? "—" : inputs.join("<br>"));
    QStringList tr_;
    for (const auto &t : d.transportNeeds) tr_ << t;
    if (d.maxCoastKm >= 0) tr_ << QStringLiteral("coastal site ≤%1 km").arg(d.maxCoastKm);
    if (d.maxRiverKm >= 0) tr_ << QStringLiteral("major river ≤%1 km").arg(d.maxRiverKm);
    if (d.needsMountain) tr_ << tr("mountain terrain");
    R(tr("Transportation & site requirements"), tr_.isEmpty() ? "—" : tr_.join(", "));
    QStringList sup;
    for (const auto &s : d.supporting) sup << QStringLiteral("%1 × '%2'%3 — %4").arg(s.count).arg(s.tag, s.radiusKm > 0 ? QStringLiteral(" within %1 km").arg(s.radiusKm) : QString(), s.why);
    for (auto it = d.techRequired.begin(); it != d.techRequired.end(); ++it) { const auto *dom = cat.techDomain(it.key()); sup << QStringLiteral("%1 maturity ≥ level %2 (now %3)").arg(dom ? dom->name : it.key()).arg(it.value()).arg(m_app->sim().world().techLevel.value(it.key())); }
    R(tr("Supporting infrastructure & ecosystem"), sup.isEmpty() ? "—" : esc(sup.join("\n")).replace("\n", "<br>"));
    QStringList out;
    if (d.generationMW > 0) out << QStringLiteral("%1 nameplate, capacity factor %2").arg(mw(d.generationMW), pct(d.capacityFactor, 0));
    if (d.storageMWh > 0) out << num(d.storageMWh) + " MWh storage";
    if (d.housingPersons > 0) out << QStringLiteral("housing for %1 people").arg(num(d.housingPersons));
    if (d.hospitalBeds > 0) out << num(d.hospitalBeds) + " hospital beds";
    if (d.primaryCareCoverage > 0) out << QStringLiteral("primary care for %1 people").arg(num(d.primaryCareCoverage));
    if (d.schoolSeats > 0) out << num(d.schoolSeats) + " school seats";
    if (d.higherEdSeats > 0) out << num(d.higherEdSeats) + " higher-education seats";
    for (int k = 0; k < SkillCount; ++k) if (d.graduatesPerYear[k] > 0) out << QStringLiteral("%1 %2 graduates/yr").arg(num(d.graduatesPerYear[k]), skillName(k).toLower());
    if (d.waterSupplyMLD > 0) out << num(d.waterSupplyMLD) + " MLD treated water";
    if (d.sewageTreatMLD > 0) out << num(d.sewageTreatMLD) + " MLD sewage treatment";
    if (d.wasteTPD > 0) out << num(d.wasteTPD) + " t/day waste processing";
    if (d.distributionMW > 0) out << mw(d.distributionMW) + " distribution capacity";
    if (d.transmissionMW > 0) out << mw(d.transmissionMW) + " transfer capacity";
    if (d.irrigationHa > 0) out << num(d.irrigationHa) + " ha irrigated";
    if (d.computeMW > 0) out << mw(d.computeMW) + " IT load";
    if (d.freightCapacityMtpa > 0) out << num(d.freightCapacityMtpa) + " Mt/yr freight";
    if (d.passengerCapacityMpa > 0) out << num(d.passengerCapacityMpa) + " million passengers/yr";
    for (const auto &g : d.outputs) { const auto *gd = cat.good(g.good); out << QStringLiteral("%1 %2/yr %3").arg(num(g.qtyPerYear), gd ? gd->unit : "", gd ? gd->name : g.good); }
    for (auto it = d.researchPoints.begin(); it != d.researchPoints.end(); ++it) { const auto *dom = cat.techDomain(it.key()); out << QStringLiteral("+%1 research points/yr (%2)").arg(num(it.value()), dom ? dom->name : it.key()); }
    R(tr("Expected production capacity"), out.isEmpty() ? "—" : out.join("<br>"));
    R(tr("Expected employment"), QStringLiteral("%1 permanent, %2 during construction").arg(num(d.operationsJobs.total()), num(d.constructionJobs)));
    QStringList em;
    if (d.emPM25 > 0) em << "PM2.5 " + num(d.emPM25) + " t/yr";
    if (d.emPM10 > 0) em << "PM10 " + num(d.emPM10) + " t/yr";
    if (d.emNOx > 0) em << "NOx " + num(d.emNOx) + " t/yr";
    if (d.emSO2 > 0) em << "SO₂ " + num(d.emSO2) + " t/yr";
    if (d.emCO > 0) em << "CO " + num(d.emCO) + " t/yr";
    if (d.emCO2 > 0) em << "CO₂ " + num(d.emCO2 / 1e6, 3) + " Mt/yr";
    R(tr("Environmental impact"), (em.isEmpty() ? tr("No significant direct air emissions modelled") : em.join(", ")) + (d.environmentNote.isEmpty() ? QString() : "<br>" + esc(d.environmentNote)));
    R(tr("Economic contribution"), (d.valueAddedCrPerYear > 0 ? cr(d.valueAddedCrPerYear) + tr(" value added per year at full output") : tr("Indirect (enables other activity)")) + (d.economicNote.isEmpty() ? QString() : "<br>" + esc(d.economicNote)));
    R(tr("Construction duration"), QStringLiteral("%1 months").arg(d.constructionMonths));
    R(tr("Maintenance / operating cost"), cr(d.opexCrPerYear) + tr(" per year ") + d.unitLabel() + (d.publicOwned ? tr(" (paid by government)") : tr(" (private operator)")));
    QStringList missing;
    EconomyEngine::techFactor(ctx, d, &missing);
    EconomyEngine::supportFactor(ctx, d, rep && rep->district >= 0 ? m_app->resources().geo.districts()[rep->district].centroid : QPointF(), &missing);
    R(tr("Missing prerequisites"), missing.isEmpty() ? tr("None at national level") : listHtml(missing));
    QString regional;
    if (!d.suitableStates.isEmpty()) regional = QStringLiteral("Suitable states: %1 <small>(%2)</small>").arg(esc(d.suitableStates.join(", ")), esc(d.suitableStatesSource));
    else regional = tr("No state restriction; see suitability assessment for site factors.");
    R(tr("Regional suitability"), regional);
    R(tr("Risks & disadvantages"), listHtml(d.risks));
    h += "</table>";
    return h;
}

QString InfoPanel::facilityHtml(const PlacedFacility &f) const
{
    const auto *d = m_app->resources().catalog.facility(f.defId);
    if (!d) return {};
    const auto &geo = m_app->resources().geo;
    QString h = QStringLiteral("<h2>%1</h2>").arg(esc(f.label.isEmpty() ? d->name : f.label));
    h += QStringLiteral("<div>%1 · %2, %3</div>").arg(esc(d->category), esc(geo.districts()[f.district].name), esc(geo.districts()[f.district].stateName));
    h += "<table>";
    if (f.status == FacilityStatus::UnderConstruction) {
        h += row(tr("Status"), QStringLiteral("<b>Under construction</b> — %1 complete").arg(pct(f.progress, 0)));
        const int left = int(std::ceil((1 - f.progress) * d->constructionMonths));
        h += row(tr("Expected completion"), QStringLiteral("≈%1 months (if funding and private capital are available)").arg(left));
    } else {
        const QString col = f.efficiency > 0.75 ? "#2a9d4b" : f.efficiency > 0.35 ? "#c98a00" : "#c0392b";
        h += row(tr("Status"), QStringLiteral("<b>Operating</b> since month %1").arg(f.completedMonth));
        h += row(tr("Output vs nameplate"), QStringLiteral("<span style='color:%1;font-size:13pt'><b>%2</b></span> %3").arg(col, pct(f.efficiency, 0), badge(DataKind::Simulated)));
        h += row(tr("Staffing"), pct(f.staffing, 0));
        h += row(tr("Electricity supply"), pct(f.powerSupply, 0));
        h += row(tr("Water supply"), pct(f.waterSupply, 0));
        h += row(tr("Inputs available"), pct(f.inputSupply, 0));
        h += row(tr("Transport access"), QStringLiteral("%1 (road %2, rail %3, port %4, air %5, fibre %6)").arg(pct(f.transportScore, 0), f.roadAccess ? "✔" : "✖", f.railAccess ? "✔" : "✖", f.portAccess ? "✔" : "✖", f.airAccess ? "✔" : "✖", f.fibreAccess ? "✔" : "✖"));
        h += row(tr("Technology readiness"), pct(f.techFactor, 0));
        h += row(tr("Supporting industry"), pct(f.supportFactor, 0));
        if (f.gridIsland >= 0) h += row(tr("Grid island"), QString::number(f.gridIsland));
        h += row(tr("Limiting factors"), listHtml(f.limitingFactors));
    }
    h += row(tr("Capital cost"), cr(f.capexCr) + QStringLiteral(" (government %1)").arg(pct(f.publicShare, 0)));
    if (f.lengthKm > 0) h += row(tr("Length"), num(f.lengthKm, 1) + " km");
    if (f.networkBroken) h += row(tr("Network"), "<span style='color:#c0392b'>Broken: river crossing without a bridge</span>");
    h += "</table><hr>";
    auto ctx = m_app->sim().context();
    return h + definitionHtml(*d, nullptr);
}

QString InfoPanel::districtHtml(int di) const
{
    const auto &g = m_app->resources().geo.districts()[di];
    const auto &d = m_app->sim().world().districts[di];
    QString h = QStringLiteral("<h2>%1</h2><div>District of %2 · %3 km²</div><table>").arg(esc(g.name), esc(g.stateName), num(g.areaKm2));
    h += row(tr("Population"), people(d.population) + " " + badge(DataKind::Simulated) + QStringLiteral("<br><small>Census 2011: %1 %2</small>").arg(people(g.census.population), badge(DataKind::Measured)));
    h += row(tr("Urban share"), pct(d.population > 0 ? d.urbanPopulation / d.population : 0));
    h += row(tr("Literacy (Census 2011)"), pct(g.census.population > 0 ? g.census.literate / g.census.population : 0) + " " + badge(DataKind::Measured));
    h += row(tr("Households with electric lighting (2011)"), pct(g.census.households > 0 ? g.census.electrifiedHouseholds / g.census.households : 0) + " " + badge(DataKind::Measured));
    h += row(tr("Labour force / unemployed"), people(d.labourForce) + " / " + people(d.unemployed) + QStringLiteral(" (%1)").arg(pct(d.labourForce > 0 ? d.unemployed / d.labourForce : 0)));
    h += row(tr("GDP (district)"), cr(d.gdpCr) + QStringLiteral(" (₹%1 per person)").arg(num(d.population > 0 ? d.gdpCr * 1e7 / d.population : 0)));
    h += row(tr("Electricity"), QStringLiteral("demand %1, served %2, distribution capacity %3").arg(mw(d.powerDemandMW), mw(d.powerServedMW), mw(d.distributionMW)));
    h += row(tr("Water"), QStringLiteral("demand %1 MLD, supplied %2 MLD").arg(num(d.waterDemandMLD, 1), num(d.waterSuppliedMLD, 1)));
    h += row(tr("Sewage treated"), QStringLiteral("%1 of %2 MLD").arg(num(d.sewageTreatedMLD, 1), num(d.sewageMLD, 1)));
    h += row(tr("Housing"), QStringLiteral("legacy %1, modern %2, shortage %3").arg(people(d.legacyHousing), people(d.modernHousing), people(std::max(0.0, d.population - d.legacyHousing - d.modernHousing))));
    h += row(tr("Health"), QStringLiteral("%1 beds; primary care for %2").arg(num(d.hospitalBeds), people(d.primaryCareCoverage)));
    h += row(tr("Schools"), num(d.schoolSeats) + tr(" seats"));
    h += row(tr("Transport"), QStringLiteral("roads %1 km, rail %2 km, access index %3").arg(num(d.roadKm), num(d.railKm), num(d.accessIndex, 2)));
    h += row(tr("Air quality (monthly mean)"), QStringLiteral("AQI <b style='color:%1'>%2 (%3)</b><br>PM2.5 %4, PM10 %5, NO₂ %6, SO₂ %7 µg/m³, CO %8 mg/m³, O₃ %9 µg/m³ %10")
             .arg(CpcbAqi::colorFor(d.aqi).darker(130).name()).arg(d.aqi).arg(CpcbAqi::categoryFor(d.aqi)).arg(num(d.pm25)).arg(num(d.pm10)).arg(num(d.no2)).arg(num(d.so2)).arg(num(d.co, 2)).arg(num(d.o3)).arg(badge(DataKind::Simulated)));
    h += row(tr("Food production"), num(d.foodProductionT) + " t/yr");
    h += "</table><p><small>District boundaries: Census 2011 (DataMeet). Simulated values are model outputs, not measurements.</small></p>";
    return h;
}

QString InfoPanel::stateHtml(int si) const
{
    const auto &s = m_app->resources().geo.states()[si];
    const auto &w = m_app->sim().world();
    double pop = 0, gdp = 0, lf = 0, un = 0, dem = 0, srv = 0, urb = 0;
    for (int di : s.districts) { const auto &d = w.districts[di]; pop += d.population; gdp += d.gdpCr; lf += d.labourForce; un += d.unemployed; dem += d.powerDemandMW; srv += d.powerServedMW; urb += d.urbanPopulation; }
    int fac = 0; for (const auto &f : w.facilities) if (f.district >= 0 && m_app->resources().geo.districts()[f.district].stateIndex == si) ++fac;
    QString h = QStringLiteral("<h2>%1</h2><div>%2 · %3 km² · %4 districts (2011 boundaries)</div><table>").arg(esc(s.name), esc(s.type), num(s.areaKm2)).arg(s.districts.size());
    h += row(tr("Population"), people(pop) + " " + badge(DataKind::Simulated) + QStringLiteral("<br><small>Census 2011: %1</small>").arg(people(s.census2011Population)));
    h += row(tr("Urban share"), pct(pop > 0 ? urb / pop : 0));
    h += row(tr("GDP (state)"), cr(gdp) + QStringLiteral(" (₹%1 per person)").arg(num(pop > 0 ? gdp * 1e7 / pop : 0)));
    h += row(tr("Unemployment"), pct(lf > 0 ? un / lf : 0));
    h += row(tr("Electricity served"), QStringLiteral("%1 of %2").arg(mw(srv), mw(dem)));
    h += row(tr("Facilities placed"), QString::number(fac));
    const auto sk = w.skills.byState.value(si);
    h += row(tr("Workforce stock"), QStringLiteral("engineers %1, scientists %2, skilled technicians %3, medical %4").arg(people(sk[Engineer]), people(sk[Scientist]), people(sk[Skilled]), people(sk[Medical])));
    h += "</table>";
    return h;
}

void InfoPanel::refresh()
{
    const auto &sel = m_app->selection();
    m_place->setVisible(sel.type == Selection::Definition);
    m_assess->setVisible(sel.type == Selection::Definition);
    m_demolish->setVisible(sel.type == Selection::Facility);
    QString html;
    switch (sel.type) {
    case Selection::Definition:
        if (const auto *d = m_app->resources().catalog.facility(sel.defId)) html = definitionHtml(*d, m_hasRep && m_repDef == sel.defId ? &m_rep : nullptr);
        break;
    case Selection::Facility:
        if (const auto *f = m_app->sim().world().facilityByUid(sel.index)) html = facilityHtml(*f);
        break;
    case Selection::District: html = districtHtml(sel.index); break;
    case Selection::State: html = stateHtml(sel.index); break;
    case Selection::None:
        html = tr("<h2>India National Development &amp; Policy Simulator</h2><p>Select a facility in the <b>Build</b> catalogue to see its full information sheet, "
                  "then place it on the map. Click a district (zoomed in) or state to see its data. Use the search box (Ctrl+F) to find any of the facility types.</p>"
                  "<p>The development layer starts empty: roads, power, cities and industries are yours to build. Physical geography and population are real-world data.</p>");
    }
    const int scroll = m_view->verticalScrollBar() ? m_view->verticalScrollBar()->value() : 0;
    m_view->setHtml(html);
    if (sel.type == Selection::Facility || sel.type == Selection::District) m_view->verticalScrollBar()->setValue(scroll);
}

} // namespace isim
