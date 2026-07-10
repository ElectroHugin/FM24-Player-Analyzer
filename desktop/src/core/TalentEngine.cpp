#include "TalentEngine.h"

#include "Definitions.h"

namespace fm {
namespace TalentEngine {

double personalityBonus(const Definitions &definitions, const QString &personality)
{
    const QString category = definitions.personalityCategory(personality);
    if (category == QLatin1String("good"))
        return kGoodPersonalityBonus;
    if (category == QLatin1String("bad"))
        return kBadPersonalityBonus;
    return 0.0;
}

double calculateTalentScore(const Definitions &definitions, double bestDwrs, double age,
                            double determination, double workRate, const QString &personality,
                            double ageCap)
{
    return bestDwrs + 2.0 * (ageCap - age) + (determination + workRate - 20.0) / 4.0
           + personalityBonus(definitions, personality);
}

double talentForPlayer(const Definitions &definitions, const Player &player, double bestDwrs,
                       double ageCap)
{
    // Legacy semantics: missing age falls back to the cap (no runway bonus).
    // Masked attribute ranges ("12-15") count as 0 — legacy parses them with
    // float()/to_numeric, which fails on ranges. Kept for golden parity.
    const double age = player.age > 0 ? player.age : ageCap;
    const auto exactValue = [&](Attr attr) {
        const int i = idx(attr);
        return player.attrLo[i] == player.attrHi[i] ? static_cast<double>(player.attrLo[i]) : 0.0;
    };
    return calculateTalentScore(definitions, bestDwrs, age, exactValue(Attr::Determination),
                                exactValue(Attr::WorkRate), player.personality, ageCap);
}

double ageCapForPlayer(const Player &player, int outfielderCap, int goalkeeperCap)
{
    return player.positionRaw.contains(QLatin1String("GK")) ? goalkeeperCap : outfielderCap;
}

} // namespace TalentEngine
} // namespace fm
