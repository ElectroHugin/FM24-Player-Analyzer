#pragma once

#include "../AppContext.h"

#include <QStringList>

#include <algorithm>

namespace fm {

// Tactic names with the (club or national) favorite tactics first — the
// legacy ordering used by Best XI, Gap Analysis and the national pages.
inline QStringList favoritesFirstTactics(AppContext &context, bool national)
{
    const QString key1 = national ? QStringLiteral("national_fav_tactic_1")
                                  : QStringLiteral("favorite_tactic_1");
    const QString key2 = national ? QStringLiteral("national_fav_tactic_2")
                                  : QStringLiteral("favorite_tactic_2");
    const QString fav1 = context.database().setting(key1);
    const QString fav2 = context.database().setting(key2);
    QStringList tactics = context.definitions().tacticNames();
    std::sort(tactics.begin(), tactics.end());
    QStringList ordered;
    if (!fav1.isEmpty() && tactics.contains(fav1))
        ordered << fav1;
    if (!fav2.isEmpty() && tactics.contains(fav2) && fav2 != fav1)
        ordered << fav2;
    for (const QString &tactic : tactics) {
        if (!ordered.contains(tactic))
            ordered << tactic;
    }
    return ordered;
}

// Uids of the saved national squad (ids live in the DB, uids in the store).
inline QSet<QString> nationalSquadUids(AppContext &context)
{
    QSet<QString> uids;
    for (const Player &player : context.store().players()) {
        if (player.inNationalSquad)
            uids.insert(player.uid);
    }
    return uids;
}

} // namespace fm
