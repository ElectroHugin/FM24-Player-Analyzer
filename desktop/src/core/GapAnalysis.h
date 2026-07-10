#pragma once

#include "SquadBuilder.h"

#include <QString>

#include <vector>

namespace fm {

// One flagged slot from the gap analysis.
struct Gap {
    QString slot;
    QString role;
    QString playerName = QStringLiteral("—"); // em dash for empty slots
    double assignedDwrs = 0.0;
    bool isDisplacement = false;
    bool isDropoff = false;
    double displacementScore = 0.0;
    double dropoff = 0.0;
    double gapScore = 0.0;
    QString bestSlot;      // empty = none
    double bestDwrs = -1.0; // -1 = none
    bool wrongSide = false;
    QString reason;
};

// Exact port of legacy gap_analysis_logic.py: flags displacement gaps (a
// player pulled below his best tactic-eligible slot or on the wrong side)
// and drop-off gaps (slot filled well below the team median DWRS). Empty
// slots always rank on top.
namespace GapAnalysis {

QString slotSide(const QString &slot); // "Left" / "Right" / "Center"

// slotOrder: the tactic's slots in definitions.json file order (legacy dict
// iteration order) — governs ordering of equal-score gaps.
std::vector<Gap> analyzeTeamGaps(const QHash<QString, XiCell> &team,
                                 const QHash<QString, QString> &positions,
                                 const QStringList &slotOrder,
                                 const QHash<QString, const Player *> &playersByUid,
                                 const RoleRatings &ratings,
                                 double displacementThreshold, double dropoffThreshold,
                                 double wrongSidePenalty);

} // namespace GapAnalysis

} // namespace fm
