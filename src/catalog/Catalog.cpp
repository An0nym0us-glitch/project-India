#include "Catalog.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace isim {

static QJsonDocument readJson(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = "Cannot open " + path; return {}; }
    QJsonParseError pe;
    auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError && error) *error = path + ": " + pe.errorString();
    return doc;
}

bool Catalog::load(const QString &dataDir, QString *error)
{
    m_facilities.clear(); m_goods.clear(); m_domains.clear();
    QDir cat(dataDir + "/catalog");
    const auto files = cat.entryList({"facilities_*.json"}, QDir::Files, QDir::Name);
    for (const auto &fn : files) {
        const auto doc = readJson(cat.filePath(fn), error);
        for (const auto &v : doc.object().value("facilities").toArray()) {
            auto d = FacilityDefinition::fromJson(v.toObject());
            if (d.id.isEmpty() || m_facIndex.contains(d.id)) {
                if (error) *error = "Duplicate or empty facility id in " + fn + ": " + d.id;
                return false;
            }
            m_facIndex.insert(d.id, m_facilities.size());
            m_facilities << d;
        }
    }
    const auto gdoc = readJson(cat.filePath("goods.json"), error);
    for (const auto &v : gdoc.object().value("goods").toArray()) {
        const auto o = v.toObject();
        GoodDefinition g;
        g.id = o.value("id").toString(); g.name = o.value("name").toString(); g.unit = o.value("unit").toString();
        g.category = o.value("category").toString();
        g.unitValueCr = o.value("unitValueCr").toDouble(); g.importability = o.value("importability").toDouble(1.0);
        g.perCapitaDemand = o.value("perCapitaDemand").toDouble(); g.incomeElasticity = o.value("incomeElasticity").toDouble();
        g.note = o.value("note").toString();
        g.provenance.kind = dataKindFromString(o.value("kind").toString("assumption"));
        g.provenance.source = o.value("source").toString();
        m_goodIndex.insert(g.id, m_goods.size());
        m_goods << g;
    }
    const auto tdoc = readJson(dataDir + "/tech/tech_tree.json", error);
    for (const auto &v : tdoc.object().value("domains").toArray()) {
        const auto o = v.toObject();
        TechDomain d;
        d.id = o.value("id").toString(); d.name = o.value("name").toString(); d.description = o.value("description").toString();
        d.initialLevel = o.value("initialLevel").toDouble(); d.initialLevelNote = o.value("initialLevelNote").toString();
        d.levels << TechLevel{}; // level 0
        d.levels[0].name = o.value("level0").toString("No domestic capability");
        int lvl = 1;
        for (const auto &lv : o.value("levels").toArray()) {
            const auto lo = lv.toObject();
            TechLevel t;
            t.level = lvl++;
            t.name = lo.value("name").toString(); t.description = lo.value("description").toString();
            t.pointsRequired = lo.value("points").toDouble();
            t.unlocks = lo.value("unlocks").toString();
            const auto pre = lo.value("prerequisites").toObject();
            for (auto it = pre.begin(); it != pre.end(); ++it) t.prerequisites.insert(it.key(), it.value().toInt());
            for (const auto &tg : lo.value("requiredFacilityTags").toArray()) t.requiredFacilityTags << tg.toString();
            d.levels << t;
        }
        m_domainIndex.insert(d.id, m_domains.size());
        m_domains << d;
    }
    m_haystack.clear();
    for (const auto &f : m_facilities) m_haystack << f.searchText();
    // validate references
    for (const auto &f : m_facilities) {
        for (const auto &g : f.inputs + f.outputs)
            if (!m_goodIndex.contains(g.good)) { if (error) *error = f.id + " references unknown good " + g.good; return false; }
        for (auto it = f.techRequired.begin(); it != f.techRequired.end(); ++it)
            if (!m_domainIndex.contains(it.key())) { if (error) *error = f.id + " references unknown tech domain " + it.key(); return false; }
        for (auto it = f.researchPoints.begin(); it != f.researchPoints.end(); ++it)
            if (!m_domainIndex.contains(it.key())) { if (error) *error = f.id + " researches unknown domain " + it.key(); return false; }
    }
    if (m_facilities.isEmpty()) { if (error && error->isEmpty()) *error = "No facility definitions found"; return false; }
    return true;
}

const FacilityDefinition *Catalog::facility(const QString &id) const
{
    const int i = m_facIndex.value(id, -1);
    return i < 0 ? nullptr : &m_facilities[i];
}
const GoodDefinition *Catalog::good(const QString &id) const
{
    const int i = m_goodIndex.value(id, -1);
    return i < 0 ? nullptr : &m_goods[i];
}
const TechDomain *Catalog::techDomain(const QString &id) const
{
    const int i = m_domainIndex.value(id, -1);
    return i < 0 ? nullptr : &m_domains[i];
}

QStringList Catalog::categories() const
{
    QStringList c;
    for (const auto &f : m_facilities) if (!c.contains(f.category)) c << f.category;
    return c;
}
QStringList Catalog::subcategories(const QString &category) const
{
    QStringList c;
    for (const auto &f : m_facilities) if (f.category == category && !c.contains(f.subcategory)) c << f.subcategory;
    return c;
}
QStringList Catalog::industries() const
{
    QStringList c;
    for (const auto &f : m_facilities) if (!f.industry.isEmpty() && !c.contains(f.industry)) c << f.industry;
    c.sort();
    return c;
}

QVector<const FacilityDefinition *> Catalog::search(const QString &text, SearchField field, const QString &category) const
{
    QVector<QPair<int, const FacilityDefinition *>> scored;
    const QStringList words = text.toLower().replace('_', ' ').split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < m_facilities.size(); ++i) {
        const auto &f = m_facilities[i];
        if (!category.isEmpty() && f.category != category) continue;
        QString hay;
        switch (field) {
        case SearchField::All: hay = m_haystack[i]; break;
        case SearchField::Name: hay = f.name.toLower(); break;
        case SearchField::Industry: hay = f.industry.toLower(); break;
        case SearchField::Technology: hay = (f.technologies.join(' ') + ' ' + QStringList(f.techRequired.keys()).join(' ')).toLower(); break;
        case SearchField::Category: hay = (f.category + ' ' + f.subcategory).toLower(); break;
        case SearchField::Function: hay = (f.function + ' ' + f.purpose).toLower(); break;
        case SearchField::Resource: {
            QStringList g;
            for (const auto &x : f.inputs) { g << x.good; if (auto gd = good(x.good)) g << gd->name; }
            hay = g.join(' ').toLower().replace('_', ' ');
            break;
        }
        }
        bool ok = true; int score = 0;
        const QString nm = f.name.toLower();
        for (const auto &w : words) {
            if (!hay.contains(w)) { ok = false; break; }
            score += nm.startsWith(w) ? 0 : nm.contains(w) ? 1 : 3;
        }
        if (ok) scored.append({score, &f});
    }
    std::stable_sort(scored.begin(), scored.end(), [](auto &a, auto &b) { return a.first < b.first; });
    QVector<const FacilityDefinition *> out;
    for (auto &s : scored) out << s.second;
    return out;
}

QVector<const FacilityDefinition *> Catalog::producersOf(const QString &good) const
{
    QVector<const FacilityDefinition *> out;
    for (const auto &f : m_facilities)
        for (const auto &o : f.outputs) if (o.good == good) { out << &f; break; }
    return out;
}

QVector<const FacilityDefinition *> Catalog::withTag(const QString &tag) const
{
    QVector<const FacilityDefinition *> out;
    for (const auto &f : m_facilities) if (f.tags.contains(tag)) out << &f;
    return out;
}

} // namespace isim
