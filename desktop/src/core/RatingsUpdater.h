#pragma once

#include "DwrsEngine.h"

#include <QString>

#include <functional>

namespace fm {

class Database;

// Port of legacy update_dwrs_ratings: recompute DWRS for players (all, or a
// subset) and append a new historical row only where the normalized value is
// new or changed by >= 1% — the append gate that keeps the history compact.
namespace RatingsUpdater {

struct Result {
    int computed = 0;  // ratings calculated
    int inserted = 0;  // rows actually appended (new or changed >= 1%)
    bool success = false;
    QString error;
};

// playersSubset: row indexes into players to restrict to (empty = all).
// progress(current, total) is invoked per role batch.
Result updateDwrsRatings(Database &db, const std::vector<Player> &players,
                         const DwrsEngine &engine, const QStringList &validRoles,
                         const std::vector<int> &playersSubset = {},
                         std::function<void(int, int)> progress = {});

} // namespace RatingsUpdater

} // namespace fm
