#pragma once
#include <algorithm>
#include "SimContext.h"
#include <array>

namespace isim {

// Cohort-component population model (17 five-year groups x 2 sexes, national),
// with district distribution, migration, urbanisation, housing and workforce stocks.
class PopulationEngine {
public:
    // advance=false only refreshes indicators (used at initialisation)
    void step(SimContext &ctx, bool advance = true);
    // Life expectancy at birth implied by Gompertz mortality with level a.
    static double lifeExpectancy(double a, double b, double child);
    static double solveMortalityLevel(double targetLE, double b, double child);
    static std::array<double, 17> mortalityRates(double a, double b, double child);
    static constexpr double kGompertzB = 0.085;   // assumption: Gompertz slope per year of age
    static double childMortality(double le) { return std::max(0.002, 0.03 - (le - 60) * 0.0012); } // assumption
private:
    double m_cachedLE = -1, m_a = 0;
    double m_tfr = -1;
};

class SkillsEngine {
public:
    void step(SimContext &ctx);
};

class TechnologyEngine {
public:
    void step(SimContext &ctx);
    // annual research points per domain at current state
    static QHash<QString, double> researchRates(const SimContext &ctx);
    static bool canAdvance(const SimContext &ctx, const QString &domain, QStringList *blockers = nullptr);
};

} // namespace isim
