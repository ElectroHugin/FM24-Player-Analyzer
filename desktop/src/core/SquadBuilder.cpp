#include "SquadBuilder.h"

#include "AppConfig.h"
#include "Constants.h"
#include "Definitions.h"
#include "TalentEngine.h"
#include "Utils.h"

#include <algorithm>
#include <limits>

namespace fm {

namespace {

struct Candidate {
    const Player *player = nullptr;
    double rating = 0.0;
    QString apt;
    double score = 0.0;
};

// Legacy youth checks parse the TEXT age and fall back to 99 when it is not a
// plain digit string; our migrator stores unparseable ages as 0.
inline int effectiveAge(const Player &p)
{
    return p.age > 0 ? p.age : 99;
}

inline bool isGoalkeeper(const Player &p)
{
    return p.positionRaw.contains(QLatin1String("GK"));
}

} // namespace

SquadBuilder::SquadBuilder(const Definitions &definitions, const AppConfig &config)
    : m_definitions(definitions)
    , m_config(config)
{
    reloadConfig();
}

void SquadBuilder::reloadConfig()
{
    m_maxDepthRoles = m_config.squadManagementSetting(QStringLiteral("max_roles_per_depth_player"));
    m_minLoanTalent = m_config.squadManagementSetting(QStringLiteral("min_loan_talent_score"));
    m_outfielderCap = m_config.ageThreshold(QStringLiteral("outfielder"));
    m_goalkeeperCap = m_config.ageThreshold(QStringLiteral("goalkeeper"));
    m_naturalPosMultiplier = m_config.selectionBonus(QStringLiteral("natural_position"));

    m_aptWeights.clear();
    for (const QString &apt : fieldPlayerAptOptions())
        m_aptWeights.insert(apt, m_config.aptWeight(apt));
    for (const QString &apt : gkAptOptions())
        m_aptWeights.insert(apt, m_config.aptWeight(apt));

    // role -> game positions it can be played from (reverse of
    // position_to_role_mapping), in the mapping's iteration order — only used
    // as a set membership test, so order is irrelevant here.
    m_roleToGamePositions.clear();
    const auto mapping = m_definitions.positionToRoleMapping();
    for (auto it = mapping.constBegin(); it != mapping.constEnd(); ++it) {
        for (const QString &role : it.value())
            m_roleToGamePositions[role].append(it.key());
    }
}

double SquadBuilder::bestDwrsForPlayer(const Player &player, const RoleRatings &ratings)
{
    double best = 0.0;
    for (const QString &role : player.assignedRoles) {
        const double rating = ratings.value(role).value(player.uid, 0.0);
        if (rating > best)
            best = rating;
    }
    return best;
}

SquadResult SquadBuilder::calculateSquadAndSurplus(const std::vector<const Player *> &pool,
                                                   const QHash<QString, QString> &positions,
                                                   const QStringList &slotOrder,
                                                   const RoleRatings &ratings,
                                                   bool applyAptWeight) const
{
    SquadResult result;

    // Parse every player's position string exactly once (legacy optimization,
    // and identical eligibility semantics).
    QHash<QString, QSet<QString>> parsedPositions;
    parsedPositions.reserve(static_cast<int>(pool.size()));
    for (const Player *p : pool)
        parsedPositions.insert(p->uid, parsePositionString(p->positionRaw));

    const auto &slotToGamePositions = tacticalSlotToGamePositions();

    // All eligible candidates for one tactical slot, best score first.
    // std::stable_sort + pool order replicates Python's stable sorted().
    const auto buildCandidates = [&](const QString &slot, const QString &role,
                                     const std::vector<const Player *> &available) {
        const QStringList allowedList = slotToGamePositions.value(slot);
        const QSet<QString> allowed(allowedList.begin(), allowedList.end());
        const QStringList naturalGamePositions = m_roleToGamePositions.value(role);
        const QHash<QString, double> roleRatings = ratings.value(role);

        std::vector<Candidate> candidates;
        for (const Player *player : available) {
            const QSet<QString> &playerPositions = parsedPositions[player->uid];
            if (!playerPositions.intersects(allowed))
                continue;
            if (!player->primaryRole.isEmpty() && player->primaryRole != role)
                continue;
            const double rating = roleRatings.value(player->uid, 0.0);
            if (rating <= 0.0)
                continue;
            const QString apt = player->agreedPlayingTime.isEmpty()
                                    ? QStringLiteral("None")
                                    : player->agreedPlayingTime;
            const double aptWeight =
                applyAptWeight ? m_aptWeights.value(apt, 1.0) : 1.0;
            double naturalBonus = 1.0;
            for (const QString &np : player->naturalPositions) {
                if (naturalGamePositions.contains(np)) {
                    naturalBonus = m_naturalPosMultiplier;
                    break;
                }
            }
            candidates.push_back({player, rating, apt, rating * aptWeight * naturalBonus});
        }
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
        return candidates;
    };

    // 'Weakest link first' team selection.
    const auto selectTeam = [&](const std::vector<const Player *> &available) {
        QHash<QString, XiCell> team;
        QSet<QString> takenUids;
        QHash<QString, std::vector<Candidate>> candidatesBySlot;
        QHash<QString, size_t> heads; // index of first not-yet-discarded candidate
        for (const QString &slot : slotOrder) {
            candidatesBySlot.insert(slot, buildCandidates(slot, positions.value(slot), available));
            heads.insert(slot, 0);
        }

        while (team.size() < slotOrder.size()) {
            QString bestSlot;
            double bestDropOff = -1.0;
            for (const QString &slot : slotOrder) { // legacy dict order
                if (team.contains(slot))
                    continue;
                auto &cands = candidatesBySlot[slot];
                size_t &head = heads[slot];
                while (head < cands.size() && takenUids.contains(cands[head].player->uid))
                    ++head;
                if (head >= cands.size())
                    continue;
                // Next untaken candidate after the head.
                double secondScore = std::numeric_limits<double>::quiet_NaN();
                for (size_t j = head + 1; j < cands.size(); ++j) {
                    if (!takenUids.contains(cands[j].player->uid)) {
                        secondScore = cands[j].score;
                        break;
                    }
                }
                const double dropOff = std::isnan(secondScore)
                                           ? std::numeric_limits<double>::infinity()
                                           : cands[head].score - secondScore;
                if (dropOff > bestDropOff) { // strict > keeps earlier slot on ties
                    bestSlot = slot;
                    bestDropOff = dropOff;
                }
            }

            if (bestSlot.isEmpty())
                break; // no more valid players for any remaining slot

            const Candidate &winner = candidatesBySlot[bestSlot][heads[bestSlot]];
            XiCell cell;
            cell.playerUid = winner.player->uid;
            cell.name = winner.player->name;
            cell.rating = std::trunc(winner.rating); // legacy: f"{int(rating)}%"
            cell.apt = winner.apt;
            team.insert(bestSlot, cell);
            takenUids.insert(winner.player->uid);
        }

        std::vector<const Player *> remaining;
        for (const Player *p : available) {
            if (!takenUids.contains(p->uid))
                remaining.push_back(p);
        }
        return std::make_pair(team, remaining);
    };

    auto [xi, remainingForBTeam] = selectTeam(pool);
    auto [bTeam, depthPool] = selectTeam(remainingForBTeam);

    // Footedness swaps on symmetrical same-role slot pairs; only EXPLICIT
    // preferred_side counts, swap only on strict improvement.
    const auto applyFootednessSwaps = [&](QHash<QString, XiCell> &team) {
        static const QList<QPair<QString, QString>> symmetricalPairs = {
            {QStringLiteral("DCL"), QStringLiteral("DCR")},
            {QStringLiteral("DMCL"), QStringLiteral("DMCR")},
            {QStringLiteral("MCL"), QStringLiteral("MCR")},
            {QStringLiteral("AMCL"), QStringLiteral("AMCR")},
            {QStringLiteral("STL"), QStringLiteral("STR")},
        };
        QHash<QString, const Player *> playerByUid;
        for (const Player *p : pool)
            playerByUid.insert(p->uid, p);

        const auto explicitSide = [](const Player *p) -> QString {
            if (p && (p->preferredSide == QLatin1String("Left")
                      || p->preferredSide == QLatin1String("Right")))
                return p->preferredSide;
            return QString();
        };

        for (const auto &[slotL, slotR] : symmetricalPairs) {
            if (!team.contains(slotL) || !team.contains(slotR))
                continue;
            if (positions.value(slotL) != positions.value(slotR))
                continue;
            const Player *playerL = playerByUid.value(team[slotL].playerUid);
            const Player *playerR = playerByUid.value(team[slotR].playerUid);
            if (!playerL || !playerR)
                continue;
            const QString prefL = explicitSide(playerL);
            const QString prefR = explicitSide(playerR);
            const int satisfiedNow = int(prefL == QLatin1String("Left"))
                                     + int(prefR == QLatin1String("Right"));
            const int satisfiedAfter = int(prefL == QLatin1String("Right"))
                                       + int(prefR == QLatin1String("Left"));
            if (satisfiedAfter > satisfiedNow)
                std::swap(team[slotL], team[slotR]);
        }
    };
    applyFootednessSwaps(xi);
    applyFootednessSwaps(bTeam);

    // Fill unassigned slots with the default placeholder cell.
    for (const QString &slot : slotOrder) {
        if (!xi.contains(slot))
            xi.insert(slot, XiCell());
        if (!bTeam.contains(slot))
            bTeam.insert(slot, XiCell());
    }

    // --- Smart depth calculation ---
    QSet<QString> xiOrBTeamUids;
    for (const XiCell &cell : std::as_const(xi)) {
        if (cell.isFilled())
            xiOrBTeamUids.insert(cell.playerUid);
    }
    for (const XiCell &cell : std::as_const(bTeam)) {
        if (cell.isFilled())
            xiOrBTeamUids.insert(cell.playerUid);
    }

    std::vector<const Player *> availableDepth;
    for (const Player *p : pool) {
        if (!xiOrBTeamUids.contains(p->uid))
            availableDepth.push_back(p);
    }

    QHash<QString, QSet<QString>> rolesCoveredByPlayer;
    if (!availableDepth.empty()) {
        QStringList uniqueRoles;
        for (const QString &slot : slotOrder) {
            const QString role = positions.value(slot);
            if (!uniqueRoles.contains(role))
                uniqueRoles.append(role);
        }
        std::sort(uniqueRoles.begin(), uniqueRoles.end());

        for (const QString &role : uniqueRoles) {
            const QHash<QString, double> roleRatings = ratings.value(role);
            std::vector<const Player *> sortedCandidates = availableDepth;
            std::stable_sort(sortedCandidates.begin(), sortedCandidates.end(),
                             [&](const Player *a, const Player *b) {
                                 return roleRatings.value(a->uid, -1.0)
                                        > roleRatings.value(b->uid, -1.0);
                             });

            const Player *bestPick = nullptr;
            for (const Player *candidate : sortedCandidates) {
                const QSet<QString> covered = rolesCoveredByPlayer.value(candidate->uid);
                if (covered.contains(role) || covered.size() < m_maxDepthRoles) {
                    bestPick = candidate;
                    break;
                }
            }
            if (!bestPick && !sortedCandidates.empty())
                bestPick = sortedCandidates.front();

            if (bestPick) {
                const double rating = roleRatings.value(bestPick->uid, 0.0);
                if (rating > 0.0) {
                    DepthOption option;
                    option.playerUid = bestPick->uid;
                    option.name = bestPick->name;
                    option.rating = std::trunc(rating);
                    option.apt = bestPick->agreedPlayingTime;
                    option.age = bestPick->age;
                    result.bestDepthOptions[role] = {option};
                    result.depthPlayerUids.insert(bestPick->uid);
                    rolesCoveredByPlayer[bestPick->uid].insert(role);
                }
            }
        }
    }

    result.startingXi = std::move(xi);
    result.bTeam = std::move(bTeam);
    result.depthPool = std::move(depthPool);
    result.coreSquadUids = xiOrBTeamUids + result.depthPlayerUids;
    return result;
}

DevelopmentSquads SquadBuilder::calculateDevelopmentSquads(
    const std::vector<const Player *> &secondTeamPlayers,
    const std::vector<const Player *> &firstTeamRemnants,
    const QHash<QString, QString> &positions, const QStringList &slotOrder,
    const RoleRatings &ratings, const QSet<QString> &depthPlayerUids) const
{
    DevelopmentSquads result;

    // 1. Second team XI (falls back to the remnant pool without a second club).
    const std::vector<const Player *> &secondPool =
        !secondTeamPlayers.empty() ? secondTeamPlayers : firstTeamRemnants;
    const SquadResult secondSquad =
        calculateSquadAndSurplus(secondPool, positions, slotOrder, ratings);
    result.secondTeamXi = secondSquad.startingXi;

    QSet<QString> secondTeamUids;
    for (const XiCell &cell : std::as_const(result.secondTeamXi)) {
        if (cell.isFilled())
            secondTeamUids.insert(cell.playerUid);
    }

    // 2. Youth XI from young remnants not already in the second XI.
    std::vector<const Player *> youthPool;
    for (const Player *p : firstTeamRemnants) {
        if (secondTeamUids.contains(p->uid))
            continue;
        const int age = effectiveAge(*p);
        const int cap = isGoalkeeper(*p) ? m_goalkeeperCap : m_outfielderCap;
        if (age <= cap)
            youthPool.push_back(p);
    }
    const SquadResult youthSquad =
        calculateSquadAndSurplus(youthPool, positions, slotOrder, ratings);
    result.youthXi = youthSquad.startingXi;

    QSet<QString> youthUids;
    for (const XiCell &cell : std::as_const(result.youthXi)) {
        if (cell.isFilled())
            youthUids.insert(cell.playerUid);
    }

    // 3. Definitive surplus: in neither XI and not kept as first-team depth.
    std::vector<const Player *> surplus;
    for (const Player *p : firstTeamRemnants) {
        if (!secondTeamUids.contains(p->uid) && !youthUids.contains(p->uid)
            && !depthPlayerUids.contains(p->uid))
            surplus.push_back(p);
    }

    // 4. Categorize: young + promising -> loan, everyone else -> sell.
    std::vector<std::pair<double, const Player *>> loanWithTalent;
    for (const Player *p : surplus) {
        const int age = effectiveAge(*p);
        const bool gk = isGoalkeeper(*p);
        const int cap = gk ? m_goalkeeperCap : m_outfielderCap;
        const bool young = age <= cap;
        if (!young) {
            result.sellCandidates.push_back(p);
            continue;
        }
        const double bestDwrs = bestDwrsForPlayer(*p, ratings);
        const double talent = TalentEngine::talentForPlayer(m_definitions, *p, bestDwrs, cap);
        if (talent >= m_minLoanTalent)
            loanWithTalent.push_back({talent, p});
        else
            result.sellCandidates.push_back(p);
    }

    std::stable_sort(loanWithTalent.begin(), loanWithTalent.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &[talent, p] : loanWithTalent)
        result.loanCandidates.push_back(p);

    std::stable_sort(result.sellCandidates.begin(), result.sellCandidates.end(),
                     [](const Player *a, const Player *b) {
                         return getLastName(a->name) < getLastName(b->name);
                     });

    return result;
}

} // namespace fm
