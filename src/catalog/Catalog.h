#pragma once
#include "FacilityDefinition.h"
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace isim {

struct GoodDefinition {
    QString id, name, unit, category;
    double unitValueCr = 0;       // ₹ crore per unit
    double importability = 1.0;   // share of shortfall that can be imported (export controls, supply)
    double perCapitaDemand = 0;   // final domestic demand per person per year (unit)
    double incomeElasticity = 0;  // demand growth vs GDP per capita
    QString note;
    Provenance provenance;
};

struct TechLevel {
    int level = 0;
    QString name, description;
    double pointsRequired = 0;
    QHash<QString, int> prerequisites;          // other domain -> level
    QStringList requiredFacilityTags;           // must have at least one operating
    QString unlocks;
};

struct TechDomain {
    QString id, name, description;
    double initialLevel = 0;
    QString initialLevelNote;
    QVector<TechLevel> levels;                  // index = level (level 0 implicit)
};

enum class SearchField { All, Name, Industry, Technology, Category, Function, Resource };

class Catalog {
public:
    bool load(const QString &dataDir, QString *error = nullptr);
    const QVector<FacilityDefinition> &facilities() const { return m_facilities; }
    const FacilityDefinition *facility(const QString &id) const;
    const GoodDefinition *good(const QString &id) const;
    const QVector<GoodDefinition> &goods() const { return m_goods; }
    const QVector<TechDomain> &techDomains() const { return m_domains; }
    const TechDomain *techDomain(const QString &id) const;
    QStringList categories() const;
    QStringList subcategories(const QString &category) const;
    QStringList industries() const;
    // Ranked search over the catalogue. Multiple words are AND-ed.
    QVector<const FacilityDefinition *> search(const QString &text, SearchField field = SearchField::All,
                                               const QString &category = QString()) const;
    // facilities whose outputs include the given good
    QVector<const FacilityDefinition *> producersOf(const QString &good) const;
    QVector<const FacilityDefinition *> withTag(const QString &tag) const;

private:
    QVector<FacilityDefinition> m_facilities;
    QVector<GoodDefinition> m_goods;
    QVector<TechDomain> m_domains;
    QHash<QString, int> m_facIndex, m_goodIndex, m_domainIndex;
    QVector<QString> m_haystack;
};

} // namespace isim
