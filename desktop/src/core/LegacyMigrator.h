#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace fm {

// Result of a legacy -> new-schema migration.
struct MigrationStats {
    int playersMigrated = 0;
    int ratingsMigrated = 0;
    int orphanedRatings = 0;   // dwrs rows whose uid has no player
    int settingsMigrated = 0;
    int nationalSquadMigrated = 0;
    int shortlistMigrated = 0;
    int orphanedSquadEntries = 0; // national_squad/shortlist uids without player
    QStringList coercions;     // human-readable notes about cleaned values (capped)
    bool success = false;
    QString error;
};

// Converts a legacy Streamlit-era SQLite database (players table with TEXT
// columns, dwrs_ratings history keyed by TEXT uid) into the new typed schema.
// Reads the source read-only, writes to a temp file next to the target and
// renames on success — the legacy DB is never modified.
class LegacyMigrator
{
public:
    // progress(current, total) is called per batch; return false from
    // shouldContinue to cancel.
    using ProgressFn = std::function<void(qint64 current, qint64 total)>;
    using ContinueFn = std::function<bool()>;

    MigrationStats migrate(const QString &legacyDbPath, const QString &targetDbPath,
                           ProgressFn progress = {}, ContinueFn shouldContinue = {});

    // Parses an attribute TEXT value: "14" -> (14,14), "12-15" -> (12,15),
    // ""/"-"/garbage -> (0,0). Returns false if the value was non-empty but
    // unparseable (caller may log a coercion).
    static bool parseAttrValue(const QString &value, int *lo, int *hi);

    // Parses a legacy stringified Python list: "['CD-D', \"DM-S\"]" -> items.
    static QStringList parseLegacyList(const QString &value);

    // "68%" -> 68.0; plain numbers pass through; garbage -> 0.
    static double parseNormalized(const QString &value);

    // "191 cm" -> 191; garbage -> 0.
    static int parseHeightCm(const QString &value);
};

} // namespace fm
