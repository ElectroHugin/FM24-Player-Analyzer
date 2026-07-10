#pragma once

#include "Player.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include <vector>

namespace fm {

class AppConfig;
class Definitions;

// Result of one role's batch calculation.
struct DwrsRoleResult {
    std::vector<double> absolute;   // aligned with the input row list
    std::vector<double> normalized; // 0-100, rounded to whole percent
};

// The DWRS ("role fit") rating engine — exact port of legacy analytics.py.
//
// Per role: every attribute in the stat-category table (GK table for GK
// roles) is weighted by the role multiplier (key/preferable/1.0), averaged
// per importance category, and summed with the category weights. The result
// is normalized to 0-100% between the theoretical worst (all attributes 1)
// and best (all attributes 20) under the same weighting.
class DwrsEngine
{
public:
    DwrsEngine(const Definitions &definitions, const AppConfig &config);

    // Rebuild cached weights/multipliers after settings or definitions change.
    void reloadConfig();

    // DWRS for a set of players (indexes into `players`) for one role.
    DwrsRoleResult calculateRole(const std::vector<Player> &players,
                                 const std::vector<int> &rows, const QString &role) const;

    // Convenience: one player, one role. Returns {absolute, normalized}.
    QPair<double, double> calculate(const Player &player, const QString &role) const;

    // All ratings for all players and every role each has assigned.
    // Returns per-role results for the row indexes that have the role.
    struct BatchResult {
        QHash<QString, std::vector<int>> roleRows;      // role -> row indexes
        QHash<QString, DwrsRoleResult> roleResults;     // role -> ratings
    };
    BatchResult calculateAllAssigned(const std::vector<Player> &players,
                                     const QStringList &validRoles) const;

private:
    struct RolePlan {
        // Per attribute participating in this role's rating:
        std::vector<int> attrIndexes;      // index into Player::attrLo/attrHi
        std::vector<double> roleWeights;   // key/pref/1.0 multiplier
        std::vector<int> categoryIndexes;  // index into categoryWeights below
        std::vector<double> categoryWeights; // weight per category (dense)
        std::vector<int> categoryCounts;     // attributes per category
        double worstPossible = 0.0;
        double bestPossible = 0.0;
    };

    RolePlan buildPlan(const QString &role) const;
    const RolePlan &planFor(const QString &role) const;

    const Definitions &m_definitions;
    const AppConfig &m_config;

    QHash<QString, double> m_weights;    // field-player category weights (from config)
    QHash<QString, double> m_gkWeights;  // GK category weights
    double m_keyMult = 1.5;
    double m_prefMult = 1.2;
    QSet<QString> m_gkRoles;

    mutable QHash<QString, RolePlan> m_planCache;
};

} // namespace fm
