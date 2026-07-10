#include "RoleAnalysis.h"

#include "Constants.h"
#include "Definitions.h"

#include <algorithm>
#include <cmath>

namespace fm {
namespace RoleAnalysis {

namespace {

// Legacy hardcodes the GK role list here (independent of definitions.json).
const QStringList &gkRolesHardcoded()
{
    static const QStringList roles = {QStringLiteral("GK-D"), QStringLiteral("SK-D"),
                                      QStringLiteral("SK-S"), QStringLiteral("SK-A")};
    return roles;
}

int tierRank(const QString &tier)
{
    if (tier == QLatin1String("personality"))
        return -1;
    if (tier == QLatin1String("key"))
        return 0;
    if (tier == QLatin1String("global"))
        return 1;
    if (tier == QLatin1String("preferable"))
        return 2;
    return 9;
}

QString article(const QString &word)
{
    if (word.isEmpty())
        return QStringLiteral("a");
    const QChar first = word.at(0).toLower();
    return QStringLiteral("aeiou").contains(first) ? QStringLiteral("an") : QStringLiteral("a");
}

} // namespace

RoleReport analyzePlayerForRole(const Definitions &definitions, const Player &player,
                                const QString &role, bool includeGlobal,
                                bool includePersonality)
{
    const RoleWeights weights = definitions.roleWeights(role);

    RoleReport report;
    report.role = role;
    report.roleName = definitions.roleDisplayMap().value(role, role);

    const auto attrValue = [&](const QString &attr) -> double {
        const int i = attrIndexByName(attr);
        return i < 0 ? 0.0 : player.attrMean(i); // range mean, missing -> 0
    };

    const auto judge = [&](const QString &attr, const QString &tier, double proThreshold,
                           double conThreshold) {
        const double value = attrValue(attr);
        if (value <= 0.0)
            return; // attribute not present in this export
        if (value >= proThreshold)
            report.pros.push_back({attr, value, tier, value >= kEliteThreshold});
        else if (value <= conThreshold)
            report.cons.push_back({attr, value, tier, false});
    };

    for (const QString &attr : weights.key)
        judge(attr, QStringLiteral("key"), kKeyProThreshold, kKeyConThreshold);
    for (const QString &attr : weights.preferable)
        judge(attr, QStringLiteral("preferable"), kPrefProThreshold, kPrefConThreshold);

    if (includeGlobal) {
        QSet<QString> already(weights.key.begin(), weights.key.end());
        for (const QString &attr : weights.preferable)
            already.insert(attr);

        const bool isGk = gkRolesHardcoded().contains(role);
        const auto &categories = isGk ? gkStatCategories() : globalStatCategories();
        const QStringList wantedTiers =
            isGk ? QStringList{QStringLiteral("Top Importance"), QStringLiteral("High Importance")}
                 : QStringList{QStringLiteral("Extremely Important"), QStringLiteral("Important")};

        for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
            if (!wantedTiers.contains(it.value()) || already.contains(it.key()))
                continue;
            judge(it.key(), QStringLiteral("global"), kKeyProThreshold, kKeyConThreshold);
        }
    }

    if (includePersonality && !player.personality.trimmed().isEmpty()) {
        const QString category = definitions.personalityCategory(player.personality);
        if (category == QLatin1String("good"))
            report.pros.push_back({player.personality, 0.0, QStringLiteral("personality"), false});
        else if (category == QLatin1String("bad"))
            report.cons.push_back({player.personality, 0.0, QStringLiteral("personality"), false});
    }

    std::stable_sort(report.pros.begin(), report.pros.end(),
                     [](const ProCon &a, const ProCon &b) {
                         const int ra = tierRank(a.tier), rb = tierRank(b.tier);
                         if (ra != rb)
                             return ra < rb;
                         return a.value > b.value;
                     });
    std::stable_sort(report.cons.begin(), report.cons.end(),
                     [](const ProCon &a, const ProCon &b) {
                         const int ra = tierRank(a.tier), rb = tierRank(b.tier);
                         if (ra != rb)
                             return ra < rb;
                         return a.value < b.value;
                     });
    return report;
}

QString formatProLine(const ProCon &pro, const QString &roleName)
{
    if (pro.tier == QLatin1String("personality"))
        return QStringLiteral("Good personality: %1").arg(pro.attr);
    const QString qualifier = pro.elite ? QStringLiteral("Elite") : QStringLiteral("Strong");
    const QString base =
        QStringLiteral("%1 %2 (%3)").arg(qualifier, pro.attr).arg(qRound(pro.value));
    if (pro.tier == QLatin1String("key"))
        return QStringLiteral("%1 for %2 %3").arg(base, article(roleName), roleName);
    return base;
}

QString formatConLine(const ProCon &con, const QString &roleName)
{
    if (con.tier == QLatin1String("personality"))
        return QStringLiteral("Poor personality: %1").arg(con.attr);
    const QString base = QStringLiteral("Low %1 (%2)").arg(con.attr).arg(qRound(con.value));
    if (con.tier == QLatin1String("key"))
        return QStringLiteral("%1 for %2 %3").arg(base, article(roleName), roleName);
    return base;
}

std::vector<TopRole> topRolesForPlayer(
    const Definitions &definitions, const Player &player,
    const QHash<QString, QHash<QString, QPair<double, double>>> &latestRatings, int limit)
{
    std::vector<TopRole> results;
    const QHash<QString, QString> displayMap = definitions.roleDisplayMap();

    for (const QString &role : player.assignedRoles) {
        const auto roleIt = latestRatings.constFind(role);
        if (roleIt == latestRatings.constEnd())
            continue;
        const auto ratingIt = roleIt->constFind(player.uid);
        if (ratingIt == roleIt->constEnd())
            continue;
        TopRole top;
        top.role = role;
        top.roleName = displayMap.value(role, role);
        top.absolute = ratingIt->first;
        top.normalized = static_cast<int>(ratingIt->second); // legacy int(float(...))
        results.push_back(std::move(top));
    }

    std::stable_sort(results.begin(), results.end(),
                     [](const TopRole &a, const TopRole &b) {
                         return a.normalized > b.normalized;
                     });
    if (static_cast<int>(results.size()) > limit)
        results.resize(limit);
    return results;
}

} // namespace RoleAnalysis
} // namespace fm
