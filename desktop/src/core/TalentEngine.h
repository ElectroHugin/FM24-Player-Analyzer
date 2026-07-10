#pragma once

#include "Player.h"

#include <QString>

namespace fm {

class Definitions;

// Talent Score — exact port of legacy talent_logic.py:
//   Talent = best DWRS of relevant roles
//          + 2 per year under the age cap
//          + (Determination + Work Rate - 20) / 4
//          + 3 good / -5 bad personality
namespace TalentEngine {

inline constexpr double kGoodPersonalityBonus = 3.0;
inline constexpr double kBadPersonalityBonus = -5.0;

double personalityBonus(const Definitions &definitions, const QString &personality);

double calculateTalentScore(const Definitions &definitions, double bestDwrs, double age,
                            double determination, double workRate, const QString &personality,
                            double ageCap);

// Convenience over a Player (uses his attribute values; bestDwrs supplied by
// the caller from the ratings matrix).
double talentForPlayer(const Definitions &definitions, const Player &player, double bestDwrs,
                       double ageCap);

// GKs develop later and use the higher cap ("GK" anywhere in the position string).
double ageCapForPlayer(const Player &player, int outfielderCap, int goalkeeperCap);

} // namespace TalentEngine

} // namespace fm
