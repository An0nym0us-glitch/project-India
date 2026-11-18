#include "WorldState.h"
#include <QJsonArray>
#include <QLocale>

namespace isim {

static QJsonArray pts(const QVector<QPointF> &p)
{
    QJsonArray a;
    for (const auto &x : p) a.append(QJsonArray{x.x(), x.y()});
    return a;
}
static QVector<QPointF> ptsFrom(const QJsonArray &a)
{
    QVector<QPointF> p;
    for (const auto &v : a) { const auto xy = v.toArray(); p << QPointF(xy.at(0).toDouble(), xy.at(1).toDouble()); }
    return p;
}
static QJsonObject hashToJson(const QHash<QString, double> &h)
{
    QJsonObject o;
    for (auto it = h.begin(); it != h.end(); ++it) o.insert(it.key(), it.value());
    return o;
}
static QHash<QString, double> hashFrom(const QJsonObject &o)
{
    QHash<QString, double> h;
    for (auto it = o.begin(); it != o.end(); ++it) h.insert(it.key(), it.value().toDouble());
    return h;
}

QJsonObject PlacedFacility::toJson() const
{
    QJsonObject o{{"uid", uid}, {"def", defId}, {"lon", location.x()}, {"lat", location.y()}, {"district", district},
                  {"lengthKm", lengthKm}, {"scale", scale}, {"status", int(status)}, {"progress", progress},
                  {"placedMonth", placedMonth}, {"completedMonth", completedMonth}, {"capexCr", capexCr},
                  {"publicShare", publicShare}, {"label", label}};
    if (!path.isEmpty()) o.insert("path", pts(path));
    QJsonArray d; for (int x : districts) d.append(x);
    o.insert("districts", d);
    return o;
}

PlacedFacility PlacedFacility::fromJson(const QJsonObject &o)
{
    PlacedFacility f;
    f.uid = o.value("uid").toInt(); f.defId = o.value("def").toString();
    f.location = QPointF(o.value("lon").toDouble(), o.value("lat").toDouble());
    f.path = ptsFrom(o.value("path").toArray());
    f.district = o.value("district").toInt(-1);
    for (const auto &v : o.value("districts").toArray()) f.districts << v.toInt();
    f.lengthKm = o.value("lengthKm").toDouble(); f.scale = o.value("scale").toDouble(1);
    f.status = FacilityStatus(o.value("status").toInt()); f.progress = o.value("progress").toDouble();
    f.placedMonth = o.value("placedMonth").toInt(); f.completedMonth = o.value("completedMonth").toInt(-1);
    f.capexCr = o.value("capexCr").toDouble(); f.publicShare = o.value("publicShare").toDouble(1);
    f.label = o.value("label").toString();
    return f;
}

QString WorldState::dateString() const
{
    return QLocale(QLocale::English).monthName(monthOfYear() + 1, QLocale::ShortFormat) + " " + QString::number(year());
}

const PlacedFacility *WorldState::facilityByUid(int uid) const
{
    for (const auto &f : facilities) if (f.uid == uid) return &f;
    return nullptr;
}
PlacedFacility *WorldState::facilityByUid(int uid)
{
    for (auto &f : facilities) if (f.uid == uid) return &f;
    return nullptr;
}

void WorldState::notify(Notification::Kind k, const QString &text)
{
    log.append({month, k, text});
    if (log.size() > 2000) log.remove(0, log.size() - 2000);
}

QJsonObject WorldState::toJson() const
{
    QJsonObject o;
    o.insert("formatVersion", formatVersion);
    o.insert("name", name); o.insert("description", description);
    o.insert("startYear", startYear); o.insert("month", month); o.insert("seed", QString::number(seed));
    QJsonArray fa; for (const auto &f : facilities) fa.append(f.toJson());
    o.insert("facilities", fa); o.insert("nextUid", nextUid);
    QJsonArray da;
    for (const auto &d : districts)
        da.append(QJsonArray{d.population, d.urbanPopulation, d.legacyHousing, d.legacyEmploymentCapacity, d.personsPerHousehold, d.legacyUrban});
    o.insert("districts", da);
    QJsonArray pm, pf; for (int i = 0; i < PopulationStructure::kGroups; ++i) { pm.append(pop.male[i]); pf.append(pop.female[i]); }
    o.insert("popMale", pm); o.insert("popFemale", pf);
    QJsonArray sk;
    for (const auto &s : skills.byState) { QJsonArray a; for (int i = 0; i < SkillCount; ++i) a.append(s[i]); sk.append(a); }
    o.insert("skills", sk);
    o.insert("techPoints", hashToJson(techPoints));
    QJsonObject tl; for (auto it = techLevel.begin(); it != techLevel.end(); ++it) tl.insert(it.key(), it.value());
    o.insert("techLevel", tl);
    QJsonArray pa;
    for (const auto &p : policies) pa.append(QJsonObject{{"id", p.id}, {"intensity", p.intensity}, {"startMonth", p.startMonth}});
    o.insert("policies", pa);
    o.insert("budget", QJsonObject{{"treasuryCr", budget.treasuryCr}, {"debtCr", budget.debtCr}});
    QJsonArray ha;
    for (const auto &h : history) ha.append(QJsonObject{{"m", h.month}, {"v", hashToJson(h.v)}});
    o.insert("history", ha);
    QJsonArray la;
    for (const auto &n : log) la.append(QJsonObject{{"m", n.month}, {"k", int(n.kind)}, {"t", n.text}});
    o.insert("log", la);
    o.insert("userSettings", hashToJson(userSettings));
    QJsonObject ad; for (auto it = advisorDecisions.begin(); it != advisorDecisions.end(); ++it) ad.insert(it.key(), it.value());
    o.insert("advisorDecisions", ad);
    o.insert("legacyProductivityIndex", legacyProductivityIndex);
    o.insert("baseLifeExpectancy", baseLifeExpectancy); o.insert("baseTfr", baseTfr);
    return o;
}

WorldState WorldState::fromJson(const QJsonObject &o, QString *error)
{
    WorldState w;
    w.formatVersion = o.value("formatVersion").toInt(1);
    if (w.formatVersion > 1 && error) *error = QStringLiteral("Scenario format version %1 is newer than supported (1)").arg(w.formatVersion);
    w.name = o.value("name").toString(); w.description = o.value("description").toString();
    w.startYear = o.value("startYear").toInt(2025); w.month = o.value("month").toInt();
    w.seed = o.value("seed").toString("42").toULongLong();
    for (const auto &v : o.value("facilities").toArray()) w.facilities << PlacedFacility::fromJson(v.toObject());
    w.nextUid = o.value("nextUid").toInt(1);
    for (const auto &v : o.value("districts").toArray()) {
        const auto a = v.toArray();
        DistrictState d;
        d.population = a.at(0).toDouble(); d.urbanPopulation = a.at(1).toDouble(); d.legacyHousing = a.at(2).toDouble();
        d.legacyEmploymentCapacity = a.at(3).toDouble(); d.personsPerHousehold = a.at(4).toDouble(4.8); d.legacyUrban = a.at(5).toDouble();
        w.districts << d;
    }
    const auto pm = o.value("popMale").toArray(), pf = o.value("popFemale").toArray();
    for (int i = 0; i < PopulationStructure::kGroups && i < pm.size(); ++i) { w.pop.male[i] = pm[i].toDouble(); w.pop.female[i] = pf[i].toDouble(); }
    for (const auto &v : o.value("skills").toArray()) {
        SkillVector s; const auto a = v.toArray();
        for (int i = 0; i < SkillCount && i < a.size(); ++i) s[i] = a[i].toDouble();
        w.skills.byState << s;
    }
    w.techPoints = hashFrom(o.value("techPoints").toObject());
    const auto tl = o.value("techLevel").toObject();
    for (auto it = tl.begin(); it != tl.end(); ++it) w.techLevel.insert(it.key(), it.value().toInt());
    for (const auto &v : o.value("policies").toArray()) {
        const auto po = v.toObject();
        w.policies.append({po.value("id").toString(), po.value("intensity").toDouble(1), po.value("startMonth").toInt(), -1});
    }
    const auto b = o.value("budget").toObject();
    w.budget.treasuryCr = b.value("treasuryCr").toDouble(); w.budget.debtCr = b.value("debtCr").toDouble();
    for (const auto &v : o.value("history").toArray()) {
        const auto ho = v.toObject();
        w.history.append({ho.value("m").toInt(), hashFrom(ho.value("v").toObject())});
    }
    for (const auto &v : o.value("log").toArray()) {
        const auto lo = v.toObject();
        w.log.append({lo.value("m").toInt(), Notification::Kind(lo.value("k").toInt()), lo.value("t").toString()});
    }
    w.userSettings = hashFrom(o.value("userSettings").toObject());
    const auto ad = o.value("advisorDecisions").toObject();
    for (auto it = ad.begin(); it != ad.end(); ++it) w.advisorDecisions.insert(it.key(), it.value().toString());
    w.legacyProductivityIndex = o.value("legacyProductivityIndex").toDouble(1);
    w.baseLifeExpectancy = o.value("baseLifeExpectancy").toDouble(); w.baseTfr = o.value("baseTfr").toDouble();
    return w;
}

} // namespace isim
