#pragma once

#include "Player.h"

#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <vector>

namespace fm {

// One historical DWRS rating row.
struct DwrsEntry {
    int playerId = 0;
    QString role;
    double absolute = 0.0;
    double normalized = 0.0; // 0-100, numeric (legacy stored "68%")
    QString timestamp;       // "yyyy-MM-dd HH:mm:ss"
};

// Latest rating per (playerId, role).
using LatestRatings = QHash<QPair<int, QString>, QPair<double, double>>; // -> (absolute, normalized)

// The new, typed SQLite schema and all persistence operations.
// Each instance owns one named connection; create one per thread.
class Database
{
public:
    // connectionName must be unique per open database+thread.
    explicit Database(const QString &connectionName);
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Opens (and creates/migrates if needed) the database file.
    bool open(const QString &filePath);
    void close();
    bool isOpen() const;
    QString errorString() const { return m_error; }
    QString filePath() const { return m_filePath; }

    // Snake_case DB column base name for an attribute ("Off the Ball" -> "off_the_ball").
    static QString attrColumnName(const QString &fullAttrName);

    // --- Players ---
    std::vector<Player> loadPlayers();

    // Inserts new players (id == 0) and updates existing ones (id != 0), in
    // one transaction. Assigns fresh ids to inserted players.
    bool upsertPlayers(std::vector<Player> &players);

    bool deletePlayers(const QList<int> &playerIds);

    // Gives an existing player a new uid (ID unification: the DB row keeps
    // its id, history and app-managed columns).
    bool renamePlayerUid(int playerId, const QString &newUid);

    // Merges a corrupted duplicate into the real record: moves the DWRS
    // history (skipping rows that would collide) and deletes the bad player.
    // App-managed field merging happens at the Player level in the importer.
    bool mergePlayerInto(int badPlayerId, int goodPlayerId);

    // --- DWRS history ---
    bool appendDwrsRatings(const std::vector<DwrsEntry> &entries);
    LatestRatings latestDwrsRatings();
    // Full history for a set of players (optionally one role), ordered by
    // player, role, timestamp.
    std::vector<DwrsEntry> dwrsHistory(const QList<int> &playerIds,
                                       const QString &role = QString());

    // --- Settings (key/value; same keys as legacy) ---
    QString setting(const QString &key, const QString &defaultValue = QString());
    bool setSetting(const QString &key, const QString &value);
    bool removeSetting(const QString &key);

    // --- National squad / shortlist (sets of player ids) ---
    QList<int> nationalSquadIds();
    bool setNationalSquadIds(const QList<int> &ids);
    QList<int> shortlistIds();
    bool setShortlistIds(const QList<int> &ids);

    // --- Maintenance ---
    // Copies the db file to <backupsDir>/<name>_backup_<ts>.db, keeps newest 3.
    static bool createBackup(const QString &dbFilePath, const QString &backupsDir,
                             QString *errorOut = nullptr);

    QSqlDatabase &handle() { return m_db; }

private:
    bool initSchema();
    bool createInitialSchema();          // fresh DB -> current shape
    bool migrateV1ToV2();                 // add dwrs_latest, drop dead columns
    static QString createDwrsLatestSql(); // shared by create + migrate paths
    bool exec(const QString &sql);

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_filePath;
    QString m_error;

    // In-memory cache of the settings table (queried several times per page
    // refresh). m_settingsLoaded marks keys whose DB state is known; a loaded
    // key absent from m_settingsCache means "no row" (returns the caller's
    // default). Kept coherent through setSetting()/removeSetting(). Per-instance
    // and therefore per-connection/thread, so no locking is needed.
    QHash<QString, QString> m_settingsCache;
    QSet<QString> m_settingsLoaded;
};

} // namespace fm
