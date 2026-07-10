#pragma once

#include "SquadBuilder.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <vector>

namespace fm {

class Definitions;

// Metrics for one tactic (legacy analyze_tactic result).
struct TacticMetrics {
    QString tactic;
    int totalSlots = 0;
    int filledSlots = 0;
    QStringList emptySlots;       // left empty by the best XI
    QStringList uncoverableSlots; // no eligible player at all
    QStringList thinSlots;        // exactly one eligible player
    double avgDepth = 0.0;        // rounded to 2 decimals
    double overallMedian = -1.0;  // rounded to 1 decimal; -1 = none
    double overallMean = -1.0;
    struct StratumStats {
        double median = -1.0;
        double mean = -1.0;
        int n = 0;
    };
    QHash<QString, StratumStats> perStratum;
    QHash<QString, int> eligibleCounts; // slot -> eligible player count
    QHash<QString, QString> positions;
    QHash<QString, XiCell> startingXi;
};

// Exact port of legacy tactic_explorer_logic.py: evaluates the pool across
// all tactics, ranking coverage-first, then median DWRS.
class TacticExplorer
{
public:
    explicit TacticExplorer(const Definitions &definitions, const SquadBuilder &builder);

    TacticMetrics analyzeTactic(const std::vector<const Player *> &pool, const QString &tactic,
                                const RoleRatings &ratings, bool applyAptWeight = true) const;

    // All tactics (or a subset), sorted by (filledSlots, overallMedian) desc,
    // ties keeping definitions.json tactic order.
    std::vector<TacticMetrics> analyzeAllTactics(const std::vector<const Player *> &pool,
                                                 const RoleRatings &ratings,
                                                 const QStringList &tactics = {},
                                                 bool applyAptWeight = true) const;

private:
    const Definitions &m_definitions;
    const SquadBuilder &m_builder;
};

} // namespace fm
