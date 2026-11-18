#pragma once
#include "core/Provenance.h"
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace isim {

// Workforce tiers used throughout the model.
enum Skill { Unskilled = 0, Skilled, Engineer, Scientist, Medical, SkillCount };
QString skillName(int s);
int skillFromString(const QString &s);

struct SkillVector {
    double v[SkillCount] = {0, 0, 0, 0, 0};
    double &operator[](int i) { return v[i]; }
    double operator[](int i) const { return v[i]; }
    double total() const { double t = 0; for (double x : v) t += x; return t; }
    SkillVector &operator+=(const SkillVector &o) { for (int i = 0; i < SkillCount; ++i) v[i] += o.v[i]; return *this; }
    SkillVector scaled(double f) const { SkillVector r; for (int i = 0; i < SkillCount; ++i) r.v[i] = v[i] * f; return r; }
};

struct GoodFlow {
    QString good;
    double qtyPerYear = 0; // in the good's unit
};

struct SupportingRequirement {
    QString tag;          // facility tag that must exist (operating)
    int count = 1;
    double radiusKm = 0;  // 0 = anywhere in India
    QString why;          // explanation shown to the user
};

enum class GeometryType { Point, Line };

// A placeable facility type, loaded from data/catalog/*.json.
// Line facilities (roads, rails, power lines...) express quantities per km.
struct FacilityDefinition {
    QString id, name, category, subcategory, industry, function;
    QStringList tags, technologies;
    QString description, purpose, environmentNote, economicNote;
    QStringList risks;
    GeometryType geometry = GeometryType::Point;
    QString network;        // for lines / network nodes: road, rail, power, water, waterway, metro, fibre
    QStringList nodeOf;     // networks this point facility is a node of (e.g. station -> rail)
    bool publicOwned = true;

    // Costs & physical requirements (per unit, or per km for lines)
    double capexCr = 0, opexCrPerYear = 0, landHa = 0;
    int constructionMonths = 12;
    double powerDemandMW = 0, waterDemandMLD = 0;
    double constructionJobs = 0;
    SkillVector operationsJobs;

    // Outputs at full efficiency
    double generationMW = 0, capacityFactor = 0, storageMWh = 0;
    double transmissionMW = 0;      // line capacity / substation transformation capacity
    double distributionMW = 0;      // district distribution capacity
    double waterSupplyMLD = 0, sewageTreatMLD = 0, wasteTPD = 0;
    double housingPersons = 0, hospitalBeds = 0, primaryCareCoverage = 0;
    double schoolSeats = 0, higherEdSeats = 0;
    SkillVector graduatesPerYear;
    double valueAddedCrPerYear = 0;
    double irrigationHa = 0, foodYieldBoost = 0;
    double freightCapacityMtpa = 0, passengerCapacityMpa = 0;
    double computeMW = 0;
    QHash<QString, double> researchPoints; // domain -> points per year
    QVector<GoodFlow> inputs, outputs;

    // Requirements
    QStringList transportNeeds;        // road, rail, port, airport, fibre
    double maxCoastKm = -1, maxRiverKm = -1;
    bool needsMountain = false, needsGrid = true, needsWater = false, needsUrban = false;
    double minGridReliability = 0;
    QStringList suitableStates;        // if non-empty, physically suitable states (resource geology etc.)
    QString suitableStatesSource;
    QHash<QString, int> techRequired;  // domain -> minimum maturity level
    QVector<SupportingRequirement> supporting;

    // Emissions (tonnes / year at full output; lines: per km)
    double emPM25 = 0, emPM10 = 0, emNOx = 0, emSO2 = 0, emCO = 0, emVOC = 0, emCO2 = 0;

    Provenance provenance;
    QString basis; // short note on how numbers were derived

    bool isLine() const { return geometry == GeometryType::Line; }
    QString unitLabel() const { return isLine() ? QStringLiteral("per km") : QStringLiteral("per facility"); }
    static FacilityDefinition fromJson(const QJsonObject &o);
    QString searchText() const; // lower-cased haystack for catalogue search
};

} // namespace isim
