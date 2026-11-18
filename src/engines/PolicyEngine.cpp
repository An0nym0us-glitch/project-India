#include "PolicyEngine.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace isim {

static QStringList sl(const QJsonValue &v) { QStringList o; for (const auto &x : v.toArray()) o << x.toString(); return o; }

bool PolicyLibrary::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = "Cannot open " + path; return false; }
    const auto arr = QJsonDocument::fromJson(f.readAll()).object().value("policies").toArray();
    m_defs.clear();
    for (const auto &v : arr) {
        const auto o = v.toObject();
        PolicyDefinition d;
        d.id = o.value("id").toString(); d.name = o.value("name").toString(); d.category = o.value("category").toString();
        d.description = o.value("description").toString(); d.intensityLabel = o.value("intensityLabel").toString();
        for (const auto &ev : o.value("effects").toArray()) {
            const auto eo = ev.toObject();
            d.effects.append({eo.value("key").toString(), eo.value("mode").toString(), eo.value("note").toString(), eo.value("full").toDouble()});
        }
        d.costCrPerYearAtFull = o.value("costCrPerYearAtFull").toDouble();
        d.costPctGdpAtFull = o.value("costPctGdpAtFull").toDouble();
        const auto e = o.value("effectiveness").toObject();
        d.effLow = e.value("low").toDouble(0.5); d.effCentral = e.value("central").toDouble(0.75); d.effHigh = e.value("high").toDouble(1.0);
        d.benefits = sl(o.value("benefits")); d.unintended = sl(o.value("unintendedConsequences"));
        d.affectedGroups = sl(o.value("affectedGroups")); d.challenges = sl(o.value("implementationChallenges"));
        d.shortTerm = o.value("shortTerm").toString(); d.longTerm = o.value("longTerm").toString();
        d.stateDifferences = o.value("stateDifferences").toString(); d.uncertainty = o.value("uncertainty").toString();
        d.humanRightsConcern = o.value("humanRightsConcern").toString(); d.revenueNote = o.value("revenueNote").toString();
        d.lagMonths = o.value("lagMonths").toInt(12); d.rampMonths = o.value("rampMonths").toInt(36);
        m_defs << d;
    }
    if (m_defs.isEmpty() && error) *error = "No policies in " + path;
    return !m_defs.isEmpty();
}

const PolicyDefinition *PolicyLibrary::find(const QString &id) const
{
    for (const auto &d : m_defs) if (d.id == id) return &d;
    return nullptr;
}

QStringList PolicyLibrary::categories() const
{
    QStringList c;
    for (const auto &d : m_defs) if (!c.contains(d.category)) c << d.category;
    return c;
}

double PolicyEngine::realisedStrength(const SimContext &ctx, const PolicyInstance &inst, const PolicyDefinition &def)
{
    const int t = ctx.w.month - inst.startMonth;
    if (t < def.lagMonths) return 0.0;
    const double ramp = def.rampMonths <= 0 ? 1.0 : std::min(1.0, double(t - def.lagMonths + 1) / def.rampMonths);
    const double eff = inst.effectiveness >= 0 ? inst.effectiveness : def.effCentral;
    return std::clamp(inst.intensity, 0.0, 1.0) * eff * ramp;
}

void PolicyEngine::computeModifiers(SimContext &ctx)
{
    ctx.mods.clear();
    // pass 1: governance trust (affects all other policy effectiveness)
    double trust = 0;
    for (const auto &inst : ctx.w.policies) {
        const auto *d = ctx.policies.find(inst.id);
        if (!d) continue;
        const double s = realisedStrength(ctx, inst, *d);
        for (const auto &e : d->effects) if (e.key == "governance.trust") trust += e.full * s;
    }
    const double trustFactor = std::clamp(1.0 + trust, 0.5, 1.3);
    for (const auto &inst : ctx.w.policies) {
        const auto *d = ctx.policies.find(inst.id);
        if (!d) continue;
        double s = realisedStrength(ctx, inst, *d);
        for (const auto &e : d->effects) {
            const double se = e.key == "governance.trust" ? s : s * trustFactor;
            if (e.mode == "mul") ctx.mods.applyMul(e.key, 1.0 + (e.full - 1.0) * se);
            else ctx.mods.applyAdd(e.key, e.full * se);
        }
    }
}

double PolicyEngine::annualCostCr(const SimContext &ctx)
{
    double cost = 0;
    for (const auto &inst : ctx.w.policies) {
        const auto *d = ctx.policies.find(inst.id);
        if (!d || ctx.w.month < inst.startMonth) continue;
        const double i = std::clamp(inst.intensity, 0.0, 1.0);
        cost += d->costCrPerYearAtFull * i + d->costPctGdpAtFull * i * ctx.w.stats.gdpCr;
    }
    return cost;
}

} // namespace isim
