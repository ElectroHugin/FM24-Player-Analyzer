#include "Database.h"

#include "Constants.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include <algorithm>

namespace fm {

namespace {

constexpr int kSchemaVersion = 1;

QString joinRolesForDb(const QStringList &roles)
{
    // player_roles junction handles roles; this helper is for natural_positions.
    return roles.join(QLatin1Char('\x1f')); // unit separator: never occurs in FM data
}

QStringList splitRolesFromDb(const QString &value)
{
    if (value.isEmpty())
        return {};
    return value.split(QLatin1Char('\x1f'), Qt::SkipEmptyParts);
}

} // namespace

QString Database::attrColumnName(const QString &fullAttrName)
{
    QString s = fullAttrName.toLower();
    s.remove(QLatin1Char('('));
    s.remove(QLatin1Char(')'));
    s.replace(QLatin1Char(' '), QLatin1Char('_'));
    // "rushing_out_(tendency)" collapses to "rushing_out_tendency" via the
    // parenthesis removal; double underscores cannot occur in our names.
    return s;
}

Database::Database(const QString &connectionName)
    : m_connectionName(connectionName)
{
}

Database::~Database()
{
    close();
}

bool Database::open(const QString &filePath)
{
    m_error.clear();
    m_filePath = filePath;

    QDir().mkpath(QFileInfo(filePath).absolutePath());

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(filePath);
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        return false;
    }
    return initSchema();
}

void Database::close()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    if (QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

bool Database::exec(const QString &sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_error = query.lastError().text() + QStringLiteral(" [SQL: ") + sql
                  + QLatin1Char(']');
        return false;
    }
    return true;
}

bool Database::initSchema()
{
    exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    QSqlQuery versionQuery(m_db);
    versionQuery.exec(QStringLiteral("PRAGMA user_version"));
    int version = 0;
    if (versionQuery.next())
        version = versionQuery.value(0).toInt();

    if (version >= kSchemaVersion)
        return true;

    // Version 0 -> 1: initial schema.
    QString attrColumns;
    for (const QString &name : attrNames()) {
        const QString base = attrColumnName(name);
        attrColumns += QStringLiteral("  %1_lo INTEGER, %1_hi INTEGER,\n").arg(base);
    }

    const QString createPlayers = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS players (\n"
        "  id INTEGER PRIMARY KEY,\n"
        "  uid TEXT NOT NULL UNIQUE,\n"
        "  name TEXT NOT NULL DEFAULT '',\n"
        "  age INTEGER,\n"
        "  club TEXT,\n"
        "  nationality TEXT,\n"
        "  second_nationality TEXT,\n"
        "  position_raw TEXT,\n"
        "  personality TEXT,\n"
        "  media_handling TEXT,\n"
        "  agreed_playing_time TEXT,\n"
        "  wage_raw TEXT,\n"
        "  transfer_value_raw TEXT,\n"
        "  transfer_value REAL,\n"
        "  av_rating REAL,\n"
        "  height_raw TEXT,\n"
        "  height_cm INTEGER,\n"
        "  left_foot TEXT,\n"
        "  right_foot TEXT,\n"
        "  preferred_foot TEXT,\n"
        "  preferred_side TEXT,\n"
        "  primary_role TEXT,\n"
        "  natural_positions TEXT,\n"
        "  transfer_status INTEGER NOT NULL DEFAULT 0,\n"
        "  loan_status INTEGER NOT NULL DEFAULT 0,\n"
        "  new_club TEXT,\n"
        "%1"
        "  registration TEXT,\n"
        "  information TEXT\n"
        ")").arg(attrColumns);

    const QStringList statements = {
        createPlayers,
        QStringLiteral("CREATE TABLE IF NOT EXISTS player_roles ("
                       " player_id INTEGER NOT NULL REFERENCES players(id) ON DELETE CASCADE,"
                       " role TEXT NOT NULL,"
                       " PRIMARY KEY (player_id, role)) WITHOUT ROWID"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS dwrs_history ("
                       " player_id INTEGER NOT NULL REFERENCES players(id) ON DELETE CASCADE,"
                       " role TEXT NOT NULL,"
                       " absolute REAL NOT NULL,"
                       " normalized REAL NOT NULL,"
                       " ts TEXT NOT NULL,"
                       " PRIMARY KEY (player_id, role, ts))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings ("
                       " key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS national_squad ("
                       " player_id INTEGER PRIMARY KEY REFERENCES players(id) ON DELETE CASCADE)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS shortlist ("
                       " player_id INTEGER PRIMARY KEY REFERENCES players(id) ON DELETE CASCADE)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_dwrs_history_role_ts ON dwrs_history(role, ts)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_player_roles_role ON player_roles(role)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_players_club ON players(club)"),
        QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion),
    };

    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    for (const QString &sql : statements) {
        if (!exec(sql)) {
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

std::vector<Player> Database::loadPlayers()
{
    std::vector<Player> players;

    // Roles first: player_id -> roles.
    QHash<int, QStringList> rolesByPlayer;
    {
        QSqlQuery query(m_db);
        query.setForwardOnly(true);
        query.exec(QStringLiteral("SELECT player_id, role FROM player_roles ORDER BY player_id, role"));
        while (query.next())
            rolesByPlayer[query.value(0).toInt()].append(query.value(1).toString());
    }
    QSet<int> nationalIds, shortlistIdSet;
    {
        QSqlQuery query(m_db);
        query.exec(QStringLiteral("SELECT player_id FROM national_squad"));
        while (query.next())
            nationalIds.insert(query.value(0).toInt());
        query.exec(QStringLiteral("SELECT player_id FROM shortlist"));
        while (query.next())
            shortlistIdSet.insert(query.value(0).toInt());
    }

    QSqlQuery query(m_db);
    query.setForwardOnly(true);
    if (!query.exec(QStringLiteral("SELECT * FROM players"))) {
        m_error = query.lastError().text();
        return players;
    }

    // Resolve column indexes once.
    const QSqlRecord record = query.record();
    const auto col = [&](const char *name) { return record.indexOf(QLatin1String(name)); };
    const int cId = col("id"), cUid = col("uid"), cName = col("name"), cAge = col("age"),
              cClub = col("club"), cNat = col("nationality"), cNat2 = col("second_nationality"),
              cPos = col("position_raw"), cPers = col("personality"),
              cMedia = col("media_handling"), cApt = col("agreed_playing_time"),
              cWage = col("wage_raw"), cTvRaw = col("transfer_value_raw"),
              cTv = col("transfer_value"), cAvr = col("av_rating"),
              cHeightRaw = col("height_raw"), cHeight = col("height_cm"),
              cLf = col("left_foot"), cRf = col("right_foot"), cPf = col("preferred_foot"),
              cSide = col("preferred_side"), cPrim = col("primary_role"),
              cNatPos = col("natural_positions"), cTs = col("transfer_status"),
              cLs = col("loan_status"), cNewClub = col("new_club");

    std::array<int, kAttrCount> loCols{}, hiCols{};
    for (int i = 0; i < kAttrCount; ++i) {
        const QString base = attrColumnName(attrNames()[i]);
        loCols[i] = record.indexOf(base + QStringLiteral("_lo"));
        hiCols[i] = record.indexOf(base + QStringLiteral("_hi"));
    }

    while (query.next()) {
        Player p;
        p.id = query.value(cId).toInt();
        p.uid = query.value(cUid).toString();
        p.name = query.value(cName).toString();
        p.age = query.value(cAge).toInt();
        p.club = query.value(cClub).toString();
        p.nationality = query.value(cNat).toString();
        p.secondNationality = query.value(cNat2).toString();
        p.positionRaw = query.value(cPos).toString();
        p.personality = query.value(cPers).toString();
        p.mediaHandling = query.value(cMedia).toString();
        p.agreedPlayingTime = query.value(cApt).toString();
        p.wageRaw = query.value(cWage).toString();
        p.transferValueRaw = query.value(cTvRaw).toString();
        p.transferValue = query.value(cTv).toDouble();
        p.averageRating = query.value(cAvr).toDouble();
        p.heightRaw = query.value(cHeightRaw).toString();
        p.heightCm = query.value(cHeight).toInt();
        p.leftFoot = query.value(cLf).toString();
        p.rightFoot = query.value(cRf).toString();
        p.preferredFoot = query.value(cPf).toString();
        p.preferredSide = query.value(cSide).toString();
        p.primaryRole = query.value(cPrim).toString();
        p.naturalPositions = splitRolesFromDb(query.value(cNatPos).toString());
        p.transferStatus = query.value(cTs).toInt() != 0;
        p.loanStatus = query.value(cLs).toInt() != 0;
        p.newClub = query.value(cNewClub).toString();
        for (int i = 0; i < kAttrCount; ++i) {
            p.attrLo[i] = static_cast<uint8_t>(query.value(loCols[i]).toInt());
            p.attrHi[i] = static_cast<uint8_t>(query.value(hiCols[i]).toInt());
        }
        p.assignedRoles = rolesByPlayer.value(p.id);
        p.inNationalSquad = nationalIds.contains(p.id);
        p.onShortlist = shortlistIdSet.contains(p.id);
        players.push_back(std::move(p));
    }
    return players;
}

bool Database::upsertPlayers(std::vector<Player> &players)
{
    if (players.empty())
        return true;

    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }

    QStringList baseColumns = {
        QStringLiteral("uid"), QStringLiteral("name"), QStringLiteral("age"),
        QStringLiteral("club"), QStringLiteral("nationality"), QStringLiteral("second_nationality"),
        QStringLiteral("position_raw"), QStringLiteral("personality"), QStringLiteral("media_handling"),
        QStringLiteral("agreed_playing_time"), QStringLiteral("wage_raw"),
        QStringLiteral("transfer_value_raw"), QStringLiteral("transfer_value"),
        QStringLiteral("av_rating"), QStringLiteral("height_raw"), QStringLiteral("height_cm"),
        QStringLiteral("left_foot"), QStringLiteral("right_foot"), QStringLiteral("preferred_foot"),
        QStringLiteral("preferred_side"), QStringLiteral("primary_role"),
        QStringLiteral("natural_positions"), QStringLiteral("transfer_status"),
        QStringLiteral("loan_status"), QStringLiteral("new_club"),
    };
    QStringList allColumns = baseColumns;
    for (const QString &name : attrNames()) {
        const QString base = attrColumnName(name);
        allColumns << base + QStringLiteral("_lo") << base + QStringLiteral("_hi");
    }

    QStringList placeholders;
    for (int i = 0; i < allColumns.size(); ++i)
        placeholders << QStringLiteral("?");

    // Upsert on the uid unique constraint keeps ids stable across re-imports.
    QStringList updateClauses;
    for (const QString &column : allColumns) {
        if (column != QLatin1String("uid"))
            updateClauses << QStringLiteral("%1=excluded.%1").arg(column);
    }

    QSqlQuery query(m_db);
    if (!query.prepare(QStringLiteral("INSERT INTO players (%1) VALUES (%2) "
                                      "ON CONFLICT(uid) DO UPDATE SET %3")
                           .arg(allColumns.join(QStringLiteral(", ")),
                                placeholders.join(QStringLiteral(", ")),
                                updateClauses.join(QStringLiteral(", "))))) {
        m_error = query.lastError().text();
        m_db.rollback();
        return false;
    }

    QSqlQuery idQuery(m_db);
    idQuery.prepare(QStringLiteral("SELECT id FROM players WHERE uid = ?"));

    QSqlQuery deleteRoles(m_db);
    deleteRoles.prepare(QStringLiteral("DELETE FROM player_roles WHERE player_id = ?"));
    QSqlQuery insertRole(m_db);
    insertRole.prepare(
        QStringLiteral("INSERT OR IGNORE INTO player_roles (player_id, role) VALUES (?, ?)"));

    for (Player &p : players) {
        int i = 0;
        query.bindValue(i++, p.uid);
        query.bindValue(i++, p.name);
        query.bindValue(i++, p.age);
        query.bindValue(i++, p.club);
        query.bindValue(i++, p.nationality);
        query.bindValue(i++, p.secondNationality);
        query.bindValue(i++, p.positionRaw);
        query.bindValue(i++, p.personality);
        query.bindValue(i++, p.mediaHandling);
        query.bindValue(i++, p.agreedPlayingTime);
        query.bindValue(i++, p.wageRaw);
        query.bindValue(i++, p.transferValueRaw);
        query.bindValue(i++, p.transferValue);
        query.bindValue(i++, p.averageRating);
        query.bindValue(i++, p.heightRaw);
        query.bindValue(i++, p.heightCm);
        query.bindValue(i++, p.leftFoot);
        query.bindValue(i++, p.rightFoot);
        query.bindValue(i++, p.preferredFoot);
        query.bindValue(i++, p.preferredSide);
        query.bindValue(i++, p.primaryRole);
        query.bindValue(i++, joinRolesForDb(p.naturalPositions));
        query.bindValue(i++, p.transferStatus ? 1 : 0);
        query.bindValue(i++, p.loanStatus ? 1 : 0);
        query.bindValue(i++, p.newClub);
        for (int a = 0; a < kAttrCount; ++a) {
            query.bindValue(i++, static_cast<int>(p.attrLo[a]));
            query.bindValue(i++, static_cast<int>(p.attrHi[a]));
        }
        if (!query.exec()) {
            m_error = query.lastError().text();
            m_db.rollback();
            return false;
        }

        if (p.id == 0) {
            idQuery.bindValue(0, p.uid);
            idQuery.exec();
            if (idQuery.next())
                p.id = idQuery.value(0).toInt();
        }

        deleteRoles.bindValue(0, p.id);
        deleteRoles.exec();
        for (const QString &role : p.assignedRoles) {
            insertRole.bindValue(0, p.id);
            insertRole.bindValue(1, role);
            insertRole.exec();
        }
    }

    return m_db.commit();
}

bool Database::deletePlayers(const QList<int> &playerIds)
{
    if (playerIds.isEmpty())
        return true;
    if (!m_db.transaction())
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM players WHERE id = ?"));
    for (const int id : playerIds) {
        query.bindValue(0, id);
        if (!query.exec()) {
            m_error = query.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

bool Database::renamePlayerUid(int playerId, const QString &newUid)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE players SET uid = ? WHERE id = ?"));
    query.bindValue(0, newUid);
    query.bindValue(1, playerId);
    if (!query.exec()) {
        m_error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::mergePlayerInto(int badPlayerId, int goodPlayerId)
{
    if (badPlayerId == goodPlayerId)
        return true;
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    // OR IGNORE skips history rows that would collide on (player, role, ts);
    // leftovers under the bad id are removed with the player row (CASCADE).
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE OR IGNORE dwrs_history SET player_id = ? WHERE player_id = ?"));
    query.bindValue(0, goodPlayerId);
    query.bindValue(1, badPlayerId);
    if (!query.exec()) {
        m_error = query.lastError().text();
        m_db.rollback();
        return false;
    }
    query.prepare(QStringLiteral("DELETE FROM players WHERE id = ?"));
    query.bindValue(0, badPlayerId);
    if (!query.exec()) {
        m_error = query.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool Database::appendDwrsRatings(const std::vector<DwrsEntry> &entries)
{
    if (entries.empty())
        return true;
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO dwrs_history (player_id, role, absolute, normalized, ts) "
        "VALUES (?, ?, ?, ?, ?)"));
    for (const DwrsEntry &entry : entries) {
        query.bindValue(0, entry.playerId);
        query.bindValue(1, entry.role);
        query.bindValue(2, entry.absolute);
        query.bindValue(3, entry.normalized);
        query.bindValue(4, entry.timestamp);
        if (!query.exec()) {
            m_error = query.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

LatestRatings Database::latestDwrsRatings()
{
    LatestRatings result;
    QSqlQuery query(m_db);
    query.setForwardOnly(true);
    // Latest ts per (player, role), then join back — same shape as legacy.
    query.exec(QStringLiteral(
        "SELECT t1.player_id, t1.role, t1.absolute, t1.normalized "
        "FROM dwrs_history t1 "
        "INNER JOIN (SELECT player_id, role, MAX(ts) AS max_ts FROM dwrs_history "
        "            GROUP BY player_id, role) t2 "
        "ON t1.player_id = t2.player_id AND t1.role = t2.role AND t1.ts = t2.max_ts"));
    while (query.next()) {
        result.insert({query.value(0).toInt(), query.value(1).toString()},
                      {query.value(2).toDouble(), query.value(3).toDouble()});
    }
    return result;
}

std::vector<DwrsEntry> Database::dwrsHistory(const QList<int> &playerIds, const QString &role)
{
    std::vector<DwrsEntry> result;
    if (playerIds.isEmpty())
        return result;

    QStringList placeholders;
    for (int i = 0; i < playerIds.size(); ++i)
        placeholders << QStringLiteral("?");

    QString sql = QStringLiteral("SELECT player_id, role, absolute, normalized, ts "
                                 "FROM dwrs_history WHERE player_id IN (%1)")
                      .arg(placeholders.join(QLatin1Char(',')));
    if (!role.isEmpty() && role != QLatin1String("All Roles"))
        sql += QStringLiteral(" AND role = ?");
    sql += QStringLiteral(" ORDER BY player_id, role, ts");

    QSqlQuery query(m_db);
    query.setForwardOnly(true);
    query.prepare(sql);
    int bindIndex = 0;
    for (const int id : playerIds)
        query.bindValue(bindIndex++, id);
    if (!role.isEmpty() && role != QLatin1String("All Roles"))
        query.bindValue(bindIndex, role);
    query.exec();

    while (query.next()) {
        DwrsEntry entry;
        entry.playerId = query.value(0).toInt();
        entry.role = query.value(1).toString();
        entry.absolute = query.value(2).toDouble();
        entry.normalized = query.value(3).toDouble();
        entry.timestamp = query.value(4).toString();
        result.push_back(std::move(entry));
    }
    return result;
}

QString Database::setting(const QString &key, const QString &defaultValue)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.bindValue(0, key);
    query.exec();
    return query.next() ? query.value(0).toString() : defaultValue;
}

bool Database::setSetting(const QString &key, const QString &value)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.bindValue(0, key);
    query.bindValue(1, value);
    if (!query.exec()) {
        m_error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::removeSetting(const QString &key)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM settings WHERE key = ?"));
    query.bindValue(0, key);
    return query.exec();
}

QList<int> Database::nationalSquadIds()
{
    QList<int> ids;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("SELECT player_id FROM national_squad"));
    while (query.next())
        ids.append(query.value(0).toInt());
    return ids;
}

bool Database::setNationalSquadIds(const QList<int> &ids)
{
    if (!m_db.transaction())
        return false;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("DELETE FROM national_squad"));
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO national_squad (player_id) VALUES (?)"));
    for (const int id : ids) {
        query.bindValue(0, id);
        query.exec();
    }
    return m_db.commit();
}

QList<int> Database::shortlistIds()
{
    QList<int> ids;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("SELECT player_id FROM shortlist"));
    while (query.next())
        ids.append(query.value(0).toInt());
    return ids;
}

bool Database::setShortlistIds(const QList<int> &ids)
{
    if (!m_db.transaction())
        return false;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("DELETE FROM shortlist"));
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO shortlist (player_id) VALUES (?)"));
    for (const int id : ids) {
        query.bindValue(0, id);
        query.exec();
    }
    return m_db.commit();
}

bool Database::createBackup(const QString &dbFilePath, const QString &backupsDir, QString *errorOut)
{
    if (!QFile::exists(dbFilePath))
        return true; // nothing to back up

    QDir().mkpath(backupsDir);
    const QString baseName = QFileInfo(dbFilePath).completeBaseName();
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    const QString backupName =
        QStringLiteral("%1_backup_%2.db").arg(baseName, timestamp);
    const QString backupPath = QDir(backupsDir).filePath(backupName);

    if (!QFile::copy(dbFilePath, backupPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Backup copy failed: %1").arg(backupPath);
        return false;
    }

    // Rotation: keep the 3 newest backups of this database.
    QStringList backups = QDir(backupsDir)
                              .entryList({QStringLiteral("%1_backup_*.db").arg(baseName)},
                                         QDir::Files, QDir::Name);
    while (backups.size() > 3) {
        QFile::remove(QDir(backupsDir).filePath(backups.takeFirst()));
    }
    return true;
}

} // namespace fm
