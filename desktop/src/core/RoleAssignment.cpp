#include "RoleAssignment.h"

#include "Database.h"
#include "Definitions.h"
#include "Utils.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace fm {

namespace RoleAssignment {

QStringList autoAssignRolesToUnassigned(Database &db, std::vector<Player> &players,
                                        const Definitions &definitions, QString *errorOut)
{
    if (errorOut)
        errorOut->clear();

    const QHash<QString, QStringList> posMap = definitions.positionToRoleMapping();

    // Position strings repeat heavily across a big scouting database; parse
    // each distinct string only once (mirrors the legacy optimization).
    QHash<QString, QStringList> rolesForPositionStr;

    std::vector<Player *> changed;
    for (Player &player : players) {
        if (!player.assignedRoles.isEmpty())
            continue;

        auto it = rolesForPositionStr.find(player.positionRaw);
        if (it == rolesForPositionStr.end()) {
            QSet<QString> roles;
            const QSet<QString> positions = parsePositionString(player.positionRaw);
            for (const QString &position : positions) {
                const QStringList mapped = posMap.value(position);
                for (const QString &role : mapped)
                    roles.insert(role);
            }
            QStringList sortedRoles(roles.cbegin(), roles.cend());
            std::sort(sortedRoles.begin(), sortedRoles.end());
            it = rolesForPositionStr.insert(player.positionRaw, sortedRoles);
        }

        if (!it.value().isEmpty()) {
            player.assignedRoles = it.value();
            changed.push_back(&player);
        }
    }

    if (changed.empty())
        return {};

    std::vector<Player> batch;
    batch.reserve(changed.size());
    for (const Player *p : changed)
        batch.push_back(*p);
    if (!db.upsertPlayers(batch)) {
        if (errorOut)
            *errorOut = db.errorString();
        // Roll the in-memory change back so store and DB stay consistent.
        for (Player *p : changed)
            p->assignedRoles.clear();
        return {};
    }

    QStringList uids;
    uids.reserve(static_cast<int>(changed.size()));
    for (const Player *p : changed)
        uids << p->uid;
    return uids;
}

} // namespace RoleAssignment

} // namespace fm
