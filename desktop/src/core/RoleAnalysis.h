#pragma once

#include "Player.h"

#include <QString>

#include <vector>

namespace fm {

class Definitions;

// Exact port of legacy role_analysis_logic.py: pros & cons of a player for a
// specific role, plus his top roles by DWRS.
namespace RoleAnalysis {

// Thresholds (legacy constants).
inline constexpr double kKeyProThreshold = 15.0;
inline constexpr double kKeyConThreshold = 9.0;
inline constexpr double kPrefProThreshold = 16.0;
inline constexpr double kPrefConThreshold = 7.0;
inline constexpr double kEliteThreshold = 18.0;

struct ProCon {
    QString attr;   // attribute name; personality entries carry the personality
    double value = 0.0;
    QString tier;   // "key" | "preferable" | "global" | "personality"
    bool elite = false;
};

struct RoleReport {
    QString role;
    QString roleName;
    std::vector<ProCon> pros; // strongest first (personality, key, global, preferable)
    std::vector<ProCon> cons; // weakest first
};

RoleReport analyzePlayerForRole(const Definitions &definitions, const Player &player,
                                const QString &role, bool includeGlobal = false,
                                bool includePersonality = false);

QString formatProLine(const ProCon &pro, const QString &roleName);
QString formatConLine(const ProCon &con, const QString &roleName);

struct TopRole {
    QString role;
    QString roleName;
    int normalized = 0;
    double absolute = 0.0;
};

// Player's best assigned roles by normalized DWRS (from a ratings lookup
// role -> uid -> (absolute, normalized)).
std::vector<TopRole> topRolesForPlayer(
    const Definitions &definitions, const Player &player,
    const QHash<QString, QHash<QString, QPair<double, double>>> &latestRatings, int limit = 5);

} // namespace RoleAnalysis

} // namespace fm
