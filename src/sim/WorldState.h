#pragma once
#include "catalog/FacilityDefinition.h"
#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <array>

namespace isim {

enum class FacilityStatus { UnderConstruction = 0, Operating = 1, Halted = 2 };

struct PlacedFacility {
    int uid = 0;
    QString defId;
    QPointF location;           // lon/lat (point facilities; first vertex for lines)
    QVector<QPointF> path;      // polyline vertices for line facilities
    int district = -1;          // district of location (lines: first vertex)
    QVector<int> districts;     // districts touched (lines)
    double lengthKm = 0;        // lines only
    double scale = 1.0;         // multiplier on unit (lines: = lengthKm)
    FacilityStatus status = FacilityStatus::UnderConstruction;
    double progress = 0;        // 0..1 construction progress
    int placedMonth = 0;
    int completedMonth = -1;
    double capexCr = 0;         // total committed cost
    double publicShare = 1.0;   // fraction financed by government
    QString label;

    // ---- runtime (recomputed every month, not saved) ----
    double efficiency = 0;      // 0..1 actual output / nameplate
    double staffing = 0, powerSupply = 0, waterSupply = 0, inputSupply = 0, transportScore = 0, techFactor = 0, supportFactor = 0;
    bool roadAccess = false, railAccess = false, portAccess = false, airAccess = false, fibreAccess = false;
    bool networkBroken = false; // line crossing a river without a bridge
    QStringList limitingFactors;
    int component = -1;         // transport component id
    int gridIsland = -1;

    bool isOperating() const { return status == FacilityStatus::Operating; }
    QJsonObject toJson() const;
    static PlacedFacility fromJson(const QJsonObject &o);
};

struct DistrictState {
    double population = 0, urbanPopulation = 0, legacyUrban = 0;
    double legacyHousing = 0;          // persons housed in pre-existing (unmodelled) stock
    double modernHousing = 0;          // persons capacity from placed residential facilities
    double personsPerHousehold = 4.8;
    double legacyEmploymentCapacity = 0;
    double formalJobs = 0, formalJobsFilled = 0, constructionJobs = 0;
    double labourForce = 0, unemployed = 0;
    double gdpCr = 0, legacyOutputCr = 0;
    // utilities
    double powerDemandMW = 0, powerServedMW = 0, distributionMW = 0;
    double waterDemandMLD = 0, waterSuppliedMLD = 0;
    double sewageMLD = 0, sewageTreatedMLD = 0;
    double wasteTPD = 0, wasteProcessedTPD = 0;
    // services
    double hospitalBeds = 0, primaryCareCoverage = 0, schoolSeats = 0, higherEdSeats = 0;
    double foodProductionT = 0, irrigationHa = 0;
    // transport
    double roadKm = 0, railKm = 0;
    bool roadAccess = false, railAccess = false;
    double accessIndex = 0;            // 0..1 transport accessibility
    // environment (monthly mean concentrations, µg/m3 except CO mg/m3)
    double pm25 = 0, pm10 = 0, no2 = 0, so2 = 0, co = 0, o3 = 0;
    int aqi = 0;
    double landUsedHa = 0;
    double attractiveness = 0;
};

// National age structure: 17 five-year groups (0-4 ... 80+), male & female.
struct PopulationStructure {
    static constexpr int kGroups = 17;
    std::array<double, kGroups> male{}, female{};
    double total() const { double t = 0; for (int i = 0; i < kGroups; ++i) t += male[i] + female[i]; return t; }
    double ages(int fromGroup, int toGroupInclusive) const {
        double t = 0; for (int i = fromGroup; i <= toGroupInclusive && i < kGroups; ++i) t += male[i] + female[i]; return t;
    }
};

struct PolicyInstance {
    QString id;
    double intensity = 1.0;   // 0..1 (or option index for discrete policies)
    int startMonth = 0;
    double effectiveness = -1; // sampled effectiveness (projections); -1 = central
};

struct Budget {
    double treasuryCr = 0, debtCr = 0;
    // flows over the last 12 months (annualised)
    double revenueCrYr = 0, capexCrYr = 0, opexCrYr = 0, policyCostCrYr = 0, interestCrYr = 0;
    double privateInvestmentCrYr = 0, privateCapitalAvailableCrYr = 0;
};

struct NationalStats {
    double population = 0, urbanShare = 0, workingAgeShare = 0, dependencyRatio = 0, lifeExpectancy = 0, tfr = 0;
    double labourForce = 0, employed = 0, unemploymentRate = 0;
    double gdpCr = 0, gdpUsd = 0, gdpPerCapitaUsd = 0, gdpGrowth = 0;
    double agricultureShare = 0, industryShare = 0, manufacturingShare = 0, servicesShare = 0;
    double exportsCr = 0, importsCr = 0;
    double powerDemandMW = 0, powerServedMW = 0, generationCapacityMW = 0, renewableCapacityMW = 0;
    double generationTWhYr = 0, renewableShareGen = 0, electricityAccess = 0, gridReliability = 0, kwhPerCapita = 0;
    double waterDemandMLD = 0, waterSuppliedMLD = 0, sewageTreatedShare = 0, wasteProcessedShare = 0;
    double housingShortagePersons = 0, housingCapacity = 0;
    double hospitalBedsPer1000 = 0, primaryCareCoverage = 0, schoolSeatShare = 0, tertiaryEnrolment = 0, literacy = 0;
    double foodSupplyRatio = 0;
    double roadKm = 0, railKm = 0, portCapacityMtpa = 0, airportCapacityMpa = 0, districtsWithRoad = 0;
    double rndShareGdp = 0, researchersPerMillion = 0, computeMW = 0, dataCentreMW = 0;
    double pm25PopWeighted = 0, co2Mt = 0, co2PerCapita = 0, aqiPopWeighted = 0;
    double inequalityTheil = 0;
    double developmentScore = 0;
    double importDependency = 0;
    double landUsedHa = 0;
};

struct Notification {
    enum Kind { Info, Construction, Economy, Environment, Warning, Advisor, Policy };
    int month = 0;
    Kind kind = Info;
    QString text;
};

struct HistoryPoint {
    int month = 0;
    QHash<QString, double> v;
};

struct SkillPools {
    QVector<SkillVector> byState; // available workforce stock per state
    SkillVector national() const { SkillVector t; for (const auto &s : byState) t += s; return t; }
};

struct WorldState {
    int formatVersion = 1;
    QString name = QStringLiteral("New scenario");
    QString description;
    int startYear = 2025;
    int month = 0; // months elapsed since January of startYear
    quint64 seed = 42;

    QVector<PlacedFacility> facilities;
    int nextUid = 1;
    QVector<DistrictState> districts;
    PopulationStructure pop;
    QVector<double> districtShare; // share of national population per district
    SkillPools skills;
    QHash<QString, double> techPoints;   // domain -> accumulated research points
    QHash<QString, int> techLevel;       // domain -> achieved maturity level
    QVector<PolicyInstance> policies;
    Budget budget;
    NationalStats stats;
    QHash<QString, double> goodsSupply, goodsDemand, goodsImports, goodsExports; // per year
    QVector<HistoryPoint> history;
    QVector<Notification> log;
    QHash<QString, double> userSettings; // scenario-level user-configured values
    QHash<QString, QString> advisorDecisions; // recommendation id -> accepted | rejected | saved
    double legacyProductivityIndex = 1.0;
    double baseLifeExpectancy = 0, baseTfr = 0;

    int year() const { return startYear + month / 12; }
    int monthOfYear() const { return month % 12; } // 0 = January
    QString dateString() const;
    const PlacedFacility *facilityByUid(int uid) const;
    PlacedFacility *facilityByUid(int uid);
    void notify(Notification::Kind k, const QString &text);

    QJsonObject toJson() const;
    static WorldState fromJson(const QJsonObject &o, QString *error = nullptr);
};

} // namespace isim
