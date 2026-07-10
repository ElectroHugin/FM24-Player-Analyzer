#include "GapAnalysis.h"

#include "Constants.h"
#include "Utils.h"

#include <algorithm>
#include <cmath>

namespace fm {
namespace GapAnalysis {

namespace {

double median(std::vector<double> values)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    if (n % 2 == 1)
        return values[n / 2];
    return (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

bool playerCanPlaySlot(const Player &player, const QString &slot)
{
    const QStringList allowed = tacticalSlotToGamePositions().value(slot);
    const QSet<QString> playerPositions = parsePositionString(player.positionRaw);
    for (const QString &pos : allowed) {
        if (playerPositions.contains(pos))
            return true;
    }
    return false;
}

QString effectiveSide(const Player &player)
{
    if (player.preferredSide == QLatin1String("Left")
        || player.preferredSide == QLatin1String("Right"))
        return player.preferredSide;
    return QString();
}

// Best slot among this tactic's slots the player is eligible for; on a DWRS
// tie the CURRENT slot wins (mirror-slot protection). Iterates slots in the
// given order (legacy dict order).
std::pair<QString, double> bestTacticSlotForPlayer(const Player &player,
                                                   const QHash<QString, QString> &positions,
                                                   const QStringList &slotOrder,
                                                   const RoleRatings &ratings,
                                                   const QString &currentSlot)
{
    QString bestSlot;
    double bestDwrs = -1.0;
    bool haveBest = false;

    for (const QString &slot : slotOrder) {
        if (!playerCanPlaySlot(player, slot))
            continue;
        const QHash<QString, double> roleRatings = ratings.value(positions.value(slot));
        const auto it = roleRatings.constFind(player.uid);
        if (it == roleRatings.constEnd())
            continue;
        const double dwrs = it.value();
        if (!haveBest || dwrs > bestDwrs) {
            bestSlot = slot;
            bestDwrs = dwrs;
            haveBest = true;
        } else if (dwrs == bestDwrs && slot == currentSlot) {
            bestSlot = slot;
        }
    }
    return {bestSlot, haveBest ? bestDwrs : -1.0};
}

} // namespace

QString slotSide(const QString &slot)
{
    if (slot.endsWith(QLatin1Char('L')))
        return QStringLiteral("Left");
    if (slot.endsWith(QLatin1Char('R')))
        return QStringLiteral("Right");
    return QStringLiteral("Center");
}

std::vector<Gap> analyzeTeamGaps(const QHash<QString, XiCell> &team,
                                 const QHash<QString, QString> &positions,
                                 const QStringList &slotOrder,
                                 const QHash<QString, const Player *> &playersByUid,
                                 const RoleRatings &ratings,
                                 double displacementThreshold, double dropoffThreshold,
                                 double wrongSidePenalty)
{
    struct Assigned {
        const Player *player;
        QString role;
        double dwrs;
    };
    QHash<QString, Assigned> assigned;
    std::vector<double> dwrsValues;

    for (const QString &slot : slotOrder) {
        const XiCell cell = team.value(slot);
        if (!cell.isFilled())
            continue;
        double dwrs = cell.rating;
        if (dwrs == 0.0)
            dwrs = ratings.value(positions.value(slot)).value(cell.playerUid, 0.0);
        const Player *player = playersByUid.value(cell.playerUid);
        if (!player)
            continue;
        assigned.insert(slot, {player, positions.value(slot), dwrs});
        dwrsValues.push_back(dwrs);
    }

    const double medianDwrs = median(dwrsValues);
    std::vector<Gap> gaps;

    for (const QString &slot : slotOrder) {
        const auto it = assigned.constFind(slot);
        if (it == assigned.constEnd())
            continue;
        const Player &player = *it->player;
        const QString &role = it->role;
        const double assignedDwrs = it->dwrs;

        // --- Displacement ---
        const auto [bestSlot, bestDwrs] =
            bestTacticSlotForPlayer(player, positions, slotOrder, ratings, slot);
        double dwrsGap = bestDwrs >= 0.0 ? bestDwrs - assignedDwrs : 0.0;
        if (dwrsGap < 0.0)
            dwrsGap = 0.0;

        // --- Wrong-side penalty (only for explicit preferences that could
        // actually be satisfied in this tactic) ---
        bool wrongSide = false;
        const QString side = effectiveSide(player);
        const QString thisSide = slotSide(slot);
        if (!side.isEmpty() && thisSide != QLatin1String("Center") && side != thisSide) {
            for (const QString &s : slotOrder) {
                if (slotSide(s) == side && playerCanPlaySlot(player, s)) {
                    wrongSide = true;
                    break;
                }
            }
        }
        const double sidePenalty = wrongSide ? wrongSidePenalty : 0.0;

        const double displacementScore = dwrsGap + sidePenalty;
        const bool isDisplacement = displacementScore >= displacementThreshold;

        // --- Drop-off ---
        double dropoff = medianDwrs - assignedDwrs;
        if (dropoff < 0.0)
            dropoff = 0.0;
        const bool isDropoff = dropoff >= dropoffThreshold;

        if (!isDisplacement && !isDropoff)
            continue;

        Gap gap;
        gap.slot = slot;
        gap.role = role;
        gap.playerName = player.name;
        gap.assignedDwrs = assignedDwrs;
        gap.isDisplacement = isDisplacement;
        gap.isDropoff = isDropoff;
        gap.displacementScore = displacementScore;
        gap.dropoff = dropoff;
        gap.gapScore = std::max(isDisplacement ? displacementScore : 0.0,
                                isDropoff ? dropoff : 0.0);
        gap.bestSlot = bestSlot;
        gap.bestDwrs = bestDwrs;
        gap.wrongSide = wrongSide;

        QStringList reasons;
        if (isDisplacement) {
            if (wrongSide && bestSlot == slot) {
                reasons << QStringLiteral("%1 is %2-sided but plays the %3 slot.")
                               .arg(player.name, side, thisSide);
            } else if (!bestSlot.isEmpty() && bestSlot != slot) {
                reasons << QStringLiteral(
                               "%1 is pulled here from %2 (%3% vs %4%) — the real gap is at %2.")
                               .arg(player.name, bestSlot)
                               .arg(qRound(bestDwrs))
                               .arg(qRound(assignedDwrs));
                if (wrongSide)
                    reasons << QStringLiteral("Also on the wrong side (%1-sided on a %2 slot).")
                                   .arg(side, thisSide);
            } else if (wrongSide) {
                reasons << QStringLiteral("%1 is %2-sided but plays the %3 slot.")
                               .arg(player.name, side, thisSide);
            }
        }
        if (isDropoff) {
            reasons << QStringLiteral("%1% is %2 below the team median (%3%).")
                           .arg(qRound(assignedDwrs))
                           .arg(qRound(dropoff))
                           .arg(qRound(medianDwrs));
        }
        gap.reason = reasons.join(QLatin1Char(' '));
        gaps.push_back(std::move(gap));
    }

    // Empty slots are always the most severe gap.
    for (const QString &slot : slotOrder) {
        if (assigned.contains(slot))
            continue;
        Gap gap;
        gap.slot = slot;
        gap.role = positions.value(slot);
        gap.isDropoff = true;
        gap.dropoff = medianDwrs;
        gap.gapScore = std::max(medianDwrs, dropoffThreshold) + 100.0;
        gap.reason = QStringLiteral("No eligible player for this slot — position is unfilled.");
        gaps.push_back(std::move(gap));
    }

    std::stable_sort(gaps.begin(), gaps.end(),
                     [](const Gap &a, const Gap &b) { return a.gapScore > b.gapScore; });
    return gaps;
}

} // namespace GapAnalysis
} // namespace fm
