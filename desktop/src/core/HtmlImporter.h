#pragma once

#include "Player.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace fm {

class Database;

// Raw table extracted from an FM HTML export.
struct HtmlTable {
    QStringList headers;
    QList<QStringList> rows;
    int malformedRows = 0; // rows whose cell count did not match the header
};

// Outcome of an HTML import (port of legacy parse_and_update_data). The
// message lists are structured facts; the UI turns them into localized
// warnings.
struct ImportResult {
    bool success = false;
    QString error;

    int rowsParsed = 0;
    int playersImported = 0;
    int newPlayers = 0;   // rows that created a player (uid not in DB before)
    int malformedRows = 0;
    int emptyUidRows = 0;

    QStringList duplicateUidNames; // duplicate UIDs inside the file (last row kept)
    QStringList unknownColumns;    // exported columns the app does not know
    // "Name (UID 123) vs. Name2 (r-123)" — incoming numeric UID matches a
    // known newgen ID but the names differ; records left untouched.
    QStringList idNameConflicts;
    // "123: 'Old Name' -> 'New Name'" — FM likely re-issued the ID.
    QStringList identityChanges;

    // Final UIDs (after ID unification) of every imported row, in file order.
    QStringList affectedUids;
};

// Parses FM HTML exports and merges them into the database. Port of legacy
// data_parser.py: header validation/deduplication, UID sanity checks and the
// ID-unification engine (missing "r-" prefix, corrupted numeric records).
class HtmlImporter
{
public:
    // Extracts the first <table> from the HTML. Returns false with errorOut
    // set if no usable table/header was found.
    static bool extractTable(const QString &html, HtmlTable *out, QString *errorOut = nullptr);

    // Full import into db. existingPlayers must reflect the current DB state
    // (ids matching); the caller reloads its stores afterwards.
    // progress(done, total) is called per processed row batch. fmVersionId
    // selects the export layout (empty = default version).
    static ImportResult importHtml(const QString &html, Database &db,
                                   const std::vector<Player> &existingPlayers,
                                   std::function<void(int, int)> progress = {},
                                   const QString &fmVersionId = QString());

    // Convenience: reads the file (UTF-8, lenient) and calls importHtml.
    static ImportResult importFile(const QString &filePath, Database &db,
                                   const std::vector<Player> &existingPlayers,
                                   std::function<void(int, int)> progress = {},
                                   const QString &fmVersionId = QString());

    // Manual, user-confirmed update of ONE player from an HTML export that
    // contains exactly one row. Writes the file's data onto targetUid
    // regardless of the UID inside the file, preserving app-managed fields.
    // Returns the player name found in the file; error set on failure.
    static QString forceUpdateSinglePlayer(const QString &html, Database &db,
                                           const std::vector<Player> &existingPlayers,
                                           const QString &targetUid, QString *errorOut,
                                           const QString &fmVersionId = QString());

    // Applies one HTML column value (full column name, e.g. "Acceleration",
    // "Transfer Value") to a player. Unknown columns are ignored.
    static void applyColumn(Player &player, const QString &fullColumnName, const QString &value);
};

} // namespace fm
