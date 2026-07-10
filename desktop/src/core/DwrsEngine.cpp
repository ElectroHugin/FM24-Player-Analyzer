#include "DwrsEngine.h"

#include "AppConfig.h"
#include "Constants.h"
#include "Definitions.h"

#include <cmath>

namespace fm {

namespace {

// numpy/python round() use round-half-to-even; std::nearbyint honors the
// default FE_TONEAREST mode which is exactly that. std::round would round
// half away from zero and break golden parity on *.5 values.
inline double roundHalfEven(double v)
{
    return std::nearbyint(v);
}

} // namespace

DwrsEngine::DwrsEngine(const Definitions &definitions, const AppConfig &config)
    : m_definitions(definitions)
    , m_config(config)
{
    reloadConfig();
}

void DwrsEngine::reloadConfig()
{
    m_weights.clear();
    const auto &fieldDefaults = weightDefaults();
    for (auto it = fieldDefaults.constBegin(); it != fieldDefaults.constEnd(); ++it)
        m_weights.insert(it.key(), m_config.weight(it.key()));

    m_gkWeights.clear();
    const auto &gkDefaults = gkWeightDefaults();
    for (auto it = gkDefaults.constBegin(); it != gkDefaults.constEnd(); ++it)
        m_gkWeights.insert(it.key(), m_config.gkWeight(it.key()));

    m_keyMult = m_config.roleMultiplier(QStringLiteral("key"));
    m_prefMult = m_config.roleMultiplier(QStringLiteral("preferable"));

    const QStringList gkRoleList = m_definitions.gkRoles();
    m_gkRoles = QSet<QString>(gkRoleList.begin(), gkRoleList.end());
    m_planCache.clear();
}

DwrsEngine::RolePlan DwrsEngine::buildPlan(const QString &role) const
{
    const bool isGk = m_gkRoles.contains(role);
    const QHash<QString, QString> &statCategories = isGk ? gkStatCategories()
                                                         : globalStatCategories();
    const QHash<QString, double> &weights = isGk ? m_gkWeights : m_weights;

    const RoleWeights roleWeights = m_definitions.roleWeights(role);
    const QSet<QString> keyAttrs(roleWeights.key.begin(), roleWeights.key.end());
    const QSet<QString> prefAttrs(roleWeights.preferable.begin(), roleWeights.preferable.end());

    RolePlan plan;

    // Dense category table. Iteration order does not affect the result: each
    // category's mean is independent, and the final sum is over categories.
    QHash<QString, int> categoryIndex;
    std::vector<double> worstSums, bestSums;

    for (auto it = statCategories.constBegin(); it != statCategories.constEnd(); ++it) {
        const QString &attr = it.key();
        const QString &category = it.value();
        const auto weightIt = weights.constFind(category);
        if (weightIt == weights.constEnd())
            continue; // category not in the weight table -> attribute ignored

        const int attrIdx = attrIndexByName(attr);
        if (attrIdx < 0)
            continue;

        int catIdx = categoryIndex.value(category, -1);
        if (catIdx < 0) {
            catIdx = static_cast<int>(plan.categoryWeights.size());
            categoryIndex.insert(category, catIdx);
            plan.categoryWeights.push_back(weightIt.value());
            plan.categoryCounts.push_back(0);
            worstSums.push_back(0.0);
            bestSums.push_back(0.0);
        }

        const double roleWeight = keyAttrs.contains(attr)    ? m_keyMult
                                  : prefAttrs.contains(attr) ? m_prefMult
                                                             : 1.0;
        plan.attrIndexes.push_back(attrIdx);
        plan.roleWeights.push_back(roleWeight);
        plan.categoryIndexes.push_back(catIdx);
        plan.categoryCounts[catIdx] += 1;
        worstSums[catIdx] += 1.0 * roleWeight;
        bestSums[catIdx] += 20.0 * roleWeight;
    }

    plan.worstPossible = 0.0;
    plan.bestPossible = 0.0;
    for (size_t c = 0; c < plan.categoryWeights.size(); ++c) {
        if (plan.categoryCounts[c] > 0) {
            plan.worstPossible += plan.categoryWeights[c] * (worstSums[c] / plan.categoryCounts[c]);
            plan.bestPossible += plan.categoryWeights[c] * (bestSums[c] / plan.categoryCounts[c]);
        }
    }
    return plan;
}

const DwrsEngine::RolePlan &DwrsEngine::planFor(const QString &role) const
{
    auto it = m_planCache.find(role);
    if (it == m_planCache.end())
        it = m_planCache.insert(role, buildPlan(role));
    return it.value();
}

DwrsRoleResult DwrsEngine::calculateRole(const std::vector<Player> &players,
                                         const std::vector<int> &rows,
                                         const QString &role) const
{
    const RolePlan &plan = planFor(role);
    const size_t n = rows.size();
    const size_t attrCount = plan.attrIndexes.size();
    const size_t catCount = plan.categoryWeights.size();

    DwrsRoleResult result;
    result.absolute.assign(n, 0.0);
    result.normalized.assign(n, 0.0);

    const double denom = plan.bestPossible - plan.worstPossible;

    std::vector<double> catSums(catCount);
    for (size_t r = 0; r < n; ++r) {
        const Player &p = players[rows[r]];
        std::fill(catSums.begin(), catSums.end(), 0.0);
        for (size_t a = 0; a < attrCount; ++a) {
            const int attrIdx = plan.attrIndexes[a];
            const double value = (p.attrLo[attrIdx] + p.attrHi[attrIdx]) / 2.0;
            catSums[plan.categoryIndexes[a]] += value * plan.roleWeights[a];
        }
        double absolute = 0.0;
        for (size_t c = 0; c < catCount; ++c) {
            if (plan.categoryCounts[c] > 0)
                absolute += plan.categoryWeights[c] * (catSums[c] / plan.categoryCounts[c]);
        }
        result.absolute[r] = absolute;
        result.normalized[r] =
            denom != 0.0 ? roundHalfEven((absolute - plan.worstPossible) / denom * 100.0) : 0.0;
    }
    return result;
}

QPair<double, double> DwrsEngine::calculate(const Player &player, const QString &role) const
{
    const std::vector<Player> single{player};
    const DwrsRoleResult result = calculateRole(single, {0}, role);
    return {result.absolute[0], result.normalized[0]};
}

DwrsEngine::BatchResult DwrsEngine::calculateAllAssigned(const std::vector<Player> &players,
                                                         const QStringList &validRoles) const
{
    BatchResult batch;
    const QSet<QString> validSet(validRoles.begin(), validRoles.end());

    for (int row = 0; row < static_cast<int>(players.size()); ++row) {
        for (const QString &role : players[row].assignedRoles) {
            if (validSet.contains(role))
                batch.roleRows[role].push_back(row);
        }
    }
    for (auto it = batch.roleRows.constBegin(); it != batch.roleRows.constEnd(); ++it)
        batch.roleResults.insert(it.key(), calculateRole(players, it.value(), it.key()));
    return batch;
}

} // namespace fm
