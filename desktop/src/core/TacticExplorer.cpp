#include "TacticExplorer.h"

#include "Constants.h"
#include "Definitions.h"
#include "Utils.h"

#include <algorithm>
#include <cmath>

namespace fm {

namespace {

// Python round(x, n) rounds half to even; nearbyint honors FE_TONEAREST.
double roundTo(double value, int decimals)
{
    const double factor = std::pow(10.0, decimals);
    return std::nearbyint(value * factor) / factor;
}

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    if (n % 2 == 1)
        return values[n / 2];
    return (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

double mean(const std::vector<double> &values)
{
    double sum = 0.0;
    for (const double v : values)
        sum += v;
    return sum / values.size();
}

QString slotToStratum(const QString &slot, const QHash<QString, QStringList> &layout)
{
    if (slot == QLatin1String("GK"))
        return QStringLiteral("Goalkeeper");
    for (auto it = layout.constBegin(); it != layout.constEnd(); ++it) {
        if (it.value().contains(slot))
            return it.key();
    }
    return QStringLiteral("Other");
}

} // namespace

TacticExplorer::TacticExplorer(const Definitions &definitions, const SquadBuilder &builder)
    : m_definitions(definitions)
    , m_builder(builder)
{
}

TacticMetrics TacticExplorer::analyzeTactic(const std::vector<const Player *> &pool,
                                            const QString &tactic, const RoleRatings &ratings,
                                            bool applyAptWeight) const
{
    const QHash<QString, QString> positions = m_definitions.tacticRoles().value(tactic);
    const QStringList slotOrder = m_definitions.tacticSlotOrder(tactic);
    const QHash<QString, QStringList> layout = m_definitions.tacticLayouts().value(tactic);

    const SquadResult squad =
        m_builder.calculateSquadAndSurplus(pool, positions, slotOrder, ratings, applyAptWeight);

    TacticMetrics metrics;
    metrics.tactic = tactic;
    metrics.totalSlots = static_cast<int>(slotOrder.size());
    metrics.positions = positions;
    metrics.startingXi = squad.startingXi;

    std::vector<double> ratingsOverall;
    QHash<QString, std::vector<double>> perStratum;

    for (const QString &slot : slotOrder) {
        const QString stratum = slotToStratum(slot, layout);
        const XiCell cell = squad.startingXi.value(slot);
        if (cell.isFilled() && cell.rating > 0.0) {
            ++metrics.filledSlots;
            ratingsOverall.push_back(cell.rating);
            perStratum[stratum].push_back(cell.rating);
        } else {
            metrics.emptySlots.append(slot);
        }
    }

    // Depth / coverage in isolation: players whose position fits the slot and
    // who have a positive rating for its role.
    const auto &slotToGamePositions = tacticalSlotToGamePositions();
    QHash<QString, QSet<QString>> parsed;
    for (const Player *p : pool)
        parsed.insert(p->uid, parsePositionString(p->positionRaw));

    double depthSum = 0.0;
    for (const QString &slot : slotOrder) {
        const QStringList allowedList = slotToGamePositions.value(slot);
        const QSet<QString> allowed(allowedList.begin(), allowedList.end());
        const QHash<QString, double> roleRatings = ratings.value(positions.value(slot));
        int count = 0;
        for (const Player *p : pool) {
            if (!parsed[p->uid].intersects(allowed))
                continue;
            if (roleRatings.value(p->uid, 0.0) > 0.0)
                ++count;
        }
        metrics.eligibleCounts.insert(slot, count);
        depthSum += count;
        if (count == 0)
            metrics.uncoverableSlots.append(slot);
        else if (count == 1)
            metrics.thinSlots.append(slot);
    }
    metrics.avgDepth =
        slotOrder.isEmpty() ? 0.0 : roundTo(depthSum / slotOrder.size(), 2);

    if (!ratingsOverall.empty()) {
        metrics.overallMedian = roundTo(median(ratingsOverall), 1);
        metrics.overallMean = roundTo(mean(ratingsOverall), 1);
    }
    for (auto it = perStratum.constBegin(); it != perStratum.constEnd(); ++it) {
        TacticMetrics::StratumStats stats;
        stats.median = roundTo(median(it.value()), 1);
        stats.mean = roundTo(mean(it.value()), 1);
        stats.n = static_cast<int>(it.value().size());
        metrics.perStratum.insert(it.key(), stats);
    }
    return metrics;
}

std::vector<TacticMetrics> TacticExplorer::analyzeAllTactics(
    const std::vector<const Player *> &pool, const RoleRatings &ratings,
    const QStringList &tactics, bool applyAptWeight) const
{
    const QStringList names =
        tactics.isEmpty() ? m_definitions.tacticNamesOrdered() : tactics;

    std::vector<TacticMetrics> results;
    for (const QString &tactic : names) {
        if (m_definitions.tacticRoles().value(tactic).isEmpty())
            continue;
        results.push_back(analyzeTactic(pool, tactic, ratings, applyAptWeight));
    }

    // Coverage first, then median; stable keeps definitions order on ties.
    std::stable_sort(results.begin(), results.end(),
                     [](const TacticMetrics &a, const TacticMetrics &b) {
                         const double medA = a.overallMedian >= 0.0 ? a.overallMedian : 0.0;
                         const double medB = b.overallMedian >= 0.0 ? b.overallMedian : 0.0;
                         if (a.filledSlots != b.filledSlots)
                             return a.filledSlots > b.filledSlots;
                         return medA > medB;
                     });
    return results;
}

} // namespace fm
