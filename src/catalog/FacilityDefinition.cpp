#include "FacilityDefinition.h"
#include <QJsonArray>

namespace isim {

QString skillName(int s)
{
    static const char *n[] = {"Unskilled / semi-skilled", "Skilled technicians", "Engineers", "Scientists & researchers", "Medical professionals"};
    return (s >= 0 && s < SkillCount) ? QString::fromLatin1(n[s]) : QString();
}

int skillFromString(const QString &s)
{
    const QString l = s.toLower();
    if (l.startsWith("unsk")) return Unskilled;
    if (l.startsWith("skill")) return Skilled;
    if (l.startsWith("eng")) return Engineer;
    if (l.startsWith("sci") || l.startsWith("res")) return Scientist;
    if (l.startsWith("med")) return Medical;
    return -1;
}

static QStringList strList(const QJsonValue &v)
{
    QStringList out;
    for (const auto &x : v.toArray()) out << x.toString();
    return out;
}

static SkillVector skills(const QJsonObject &o)
{
    SkillVector s;
    for (auto it = o.begin(); it != o.end(); ++it) {
        const int k = skillFromString(it.key());
        if (k >= 0) s[k] = it.value().toDouble();
    }
    return s;
}

static QVector<GoodFlow> flows(const QJsonValue &v)
{
    QVector<GoodFlow> out;
    for (const auto &x : v.toArray()) {
        const auto o = x.toObject();
        out.append({o.value("good").toString(), o.value("qtyPerYear").toDouble()});
    }
    return out;
}

FacilityDefinition FacilityDefinition::fromJson(const QJsonObject &o)
{
    FacilityDefinition d;
    d.id = o.value("id").toString();
    d.name = o.value("name").toString();
    d.category = o.value("category").toString();
    d.subcategory = o.value("subcategory").toString();
    d.industry = o.value("industry").toString();
    d.function = o.value("function").toString();
    d.tags = strList(o.value("tags"));
    d.technologies = strList(o.value("technologies"));
    d.description = o.value("description").toString();
    d.purpose = o.value("purpose").toString();
    d.environmentNote = o.value("environmentNote").toString();
    d.economicNote = o.value("economicNote").toString();
    d.risks = strList(o.value("risks"));
    d.geometry = o.value("geometry").toString() == "line" ? GeometryType::Line : GeometryType::Point;
    d.network = o.value("network").toString();
    d.nodeOf = strList(o.value("nodeOf"));
    d.publicOwned = o.value("publicOwned").toBool(true);
    d.capexCr = o.value("capexCr").toDouble();
    d.opexCrPerYear = o.value("opexCrPerYear").toDouble();
    d.landHa = o.value("landHa").toDouble();
    d.constructionMonths = o.value("constructionMonths").toInt(12);
    d.powerDemandMW = o.value("powerDemandMW").toDouble();
    d.waterDemandMLD = o.value("waterDemandMLD").toDouble();
    d.constructionJobs = o.value("constructionJobs").toDouble();
    d.operationsJobs = skills(o.value("operationsJobs").toObject());

    const auto out = o.value("outputs").toObject();
    d.generationMW = out.value("generationMW").toDouble();
    d.capacityFactor = out.value("capacityFactor").toDouble();
    d.storageMWh = out.value("storageMWh").toDouble();
    d.transmissionMW = out.value("transmissionMW").toDouble();
    d.distributionMW = out.value("distributionMW").toDouble();
    d.waterSupplyMLD = out.value("waterSupplyMLD").toDouble();
    d.sewageTreatMLD = out.value("sewageTreatMLD").toDouble();
    d.wasteTPD = out.value("wasteTPD").toDouble();
    d.housingPersons = out.value("housingPersons").toDouble();
    d.hospitalBeds = out.value("hospitalBeds").toDouble();
    d.primaryCareCoverage = out.value("primaryCareCoverage").toDouble();
    d.schoolSeats = out.value("schoolSeats").toDouble();
    d.higherEdSeats = out.value("higherEdSeats").toDouble();
    d.graduatesPerYear = skills(out.value("graduatesPerYear").toObject());
    d.valueAddedCrPerYear = out.value("valueAddedCrPerYear").toDouble();
    d.irrigationHa = out.value("irrigationHa").toDouble();
    d.foodYieldBoost = out.value("foodYieldBoost").toDouble();
    d.freightCapacityMtpa = out.value("freightCapacityMtpa").toDouble();
    d.passengerCapacityMpa = out.value("passengerCapacityMpa").toDouble();
    d.computeMW = out.value("computeMW").toDouble();
    const auto rp = out.value("researchPoints").toObject();
    for (auto it = rp.begin(); it != rp.end(); ++it) d.researchPoints.insert(it.key(), it.value().toDouble());
    d.outputs = flows(out.value("goods"));
    d.inputs = flows(o.value("inputs"));

    const auto rq = o.value("requires").toObject();
    d.transportNeeds = strList(rq.value("transport"));
    d.maxCoastKm = rq.value("maxCoastKm").toDouble(-1);
    d.maxRiverKm = rq.value("maxRiverKm").toDouble(-1);
    d.needsMountain = rq.value("mountain").toBool(false);
    d.needsGrid = rq.value("grid").toBool(d.powerDemandMW > 0);
    d.needsWater = rq.value("water").toBool(d.waterDemandMLD > 0.5);
    d.needsUrban = rq.value("urban").toBool(false);
    d.minGridReliability = rq.value("minGridReliability").toDouble(0);
    d.suitableStates = strList(rq.value("states"));
    d.suitableStatesSource = rq.value("statesSource").toString();
    const auto tech = rq.value("tech").toObject();
    for (auto it = tech.begin(); it != tech.end(); ++it) d.techRequired.insert(it.key(), it.value().toInt());
    for (const auto &sv : rq.value("supporting").toArray()) {
        const auto so = sv.toObject();
        d.supporting.append({so.value("tag").toString(), so.value("count").toInt(1), so.value("radiusKm").toDouble(0), so.value("why").toString()});
    }
    const auto em = o.value("emissions").toObject();
    d.emPM25 = em.value("pm25").toDouble(); d.emPM10 = em.value("pm10").toDouble();
    d.emNOx = em.value("nox").toDouble(); d.emSO2 = em.value("so2").toDouble();
    d.emCO = em.value("co").toDouble(); d.emVOC = em.value("voc").toDouble(); d.emCO2 = em.value("co2").toDouble();
    const auto pv = o.value("provenance").toObject();
    d.provenance.kind = dataKindFromString(pv.value("kind").toString("assumption"));
    d.provenance.source = pv.value("source").toString();
    d.provenance.confidence = pv.value("confidence").toString("low");
    d.basis = pv.value("basis").toString();
    return d;
}

QString FacilityDefinition::searchText() const
{
    QStringList parts{name, category, subcategory, industry, function, description, network};
    parts << tags << technologies;
    for (const auto &g : inputs) parts << g.good;
    for (const auto &g : outputs) parts << g.good;
    for (auto it = techRequired.begin(); it != techRequired.end(); ++it) parts << it.key();
    for (auto it = researchPoints.begin(); it != researchPoints.end(); ++it) parts << it.key();
    return parts.join(' ').toLower().replace('_', ' ');
}

} // namespace isim
