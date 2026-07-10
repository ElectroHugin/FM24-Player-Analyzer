#include "LegacyMigrator.h"

#include "Constants.h"
#include "Database.h"
#include "Utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>

namespace fm {

namespace {

constexpr int kBatchSize = 5000;
constexpr int kMaxCoercionNotes = 200;

void note(MigrationStats &stats, const QString &message)
{
    if (stats.coercions.size() < kMaxCoercionNotes)
        stats.coercions.append(message);
}

} // namespace

bool LegacyMigrator::parseAttrValue(const QString &value, int *lo, int *hi)
{
    *lo = 0;
    *hi = 0;
    const QString s = value.trimmed();
    if (s.isEmpty() || s == QLatin1String("-"))
        return true;

    const int dash = s.indexOf(QLatin1Char('-'));
    bool okLo = false, okHi = false;
    if (dash > 0) {
        const int l = s.left(dash).trimmed().toInt(&okLo);
        const int h = s.mid(dash + 1).trimmed().toInt(&okHi);
        if (okLo && okHi && l >= 1 && h <= 20 && l <= h) {
            *lo = l;
            *hi = h;
            return true;
        }
        return false;
    }

    const int v = s.toInt(&okLo);
    if (okLo && v >= 1 && v <= 20) {
        *lo = v;
        *hi = v;
        return true;
    }
    return false;
}

QStringList LegacyMigrator::parseLegacyList(const QString &value)
{
    QStringList items;
    const QString s = value.trimmed();
    if (s.isEmpty() || s == QLatin1String("[]") || s == QLatin1String("None"))
        return items;
    if (!s.startsWith(QLatin1Char('[')) || !s.endsWith(QLatin1Char(']')))
        return items;

    // Tolerant scan of a Python list literal of strings: ['a', "b, c", ...]
    const QString inner = s.mid(1, s.size() - 2);
    int i = 0;
    const int n = inner.size();
    while (i < n) {
        // Skip whitespace and commas.
        while (i < n && (inner[i].isSpace() || inner[i] == QLatin1Char(',')))
            ++i;
        if (i >= n)
            break;
        const QChar quote = inner[i];
        if (quote != QLatin1Char('\'') && quote != QLatin1Char('"')) {
            // Not a quoted string — take the bare token up to the next comma.
            int end = inner.indexOf(QLatin1Char(','), i);
            if (end < 0)
                end = n;
            const QString token = inner.mid(i, end - i).trimmed();
            if (!token.isEmpty())
                items.append(token);
            i = end;
            continue;
        }
        ++i; // past opening quote
        QString item;
        while (i < n) {
            if (inner[i] == QLatin1Char('\\') && i + 1 < n) {
                item.append(inner[i + 1]);
                i += 2;
                continue;
            }
            if (inner[i] == quote)
                break;
            item.append(inner[i]);
            ++i;
        }
        ++i; // past closing quote
        items.append(item);
    }
    return items;
}

double LegacyMigrator::parseNormalized(const QString &value)
{
    QString s = value.trimmed();
    if (s.endsWith(QLatin1Char('%')))
        s.chop(1);
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

int LegacyMigrator::parseHeightCm(const QString &value)
{
    QString s = value.trimmed();
    s.remove(QLatin1String("cm"));
    s = s.trimmed();
    bool ok = false;
    const int v = s.toInt(&ok);
    return ok ? v : 0;
}

MigrationStats LegacyMigrator::migrate(const QString &legacyDbPath, const QString &targetDbPath,
                                       ProgressFn progress, ContinueFn shouldContinue)
{
    MigrationStats stats;

    if (!QFile::exists(legacyDbPath)) {
        stats.error = QStringLiteral("Legacy database not found: %1").arg(legacyDbPath);
        return stats;
    }

    const QString tempPath = targetDbPath + QStringLiteral(".migrating");
    QFile::remove(tempPath);

    const QString srcConn =
        QStringLiteral("legacy_src_%1").arg(QUuid::createUuid().toString(QUuid::Id128));

    {
        QSqlDatabase src = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), srcConn);
        src.setDatabaseName(legacyDbPath);
        src.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!src.open()) {
            stats.error = QStringLiteral("Cannot open legacy DB: %1").arg(src.lastError().text());
            QSqlDatabase::removeDatabase(srcConn);
            return stats;
        }

        Database target(QStringLiteral("migrate_target_%1")
                            .arg(QUuid::createUuid().toString(QUuid::Id128)));
        if (!target.open(tempPath)) {
            stats.error = QStringLiteral("Cannot create target DB: %1").arg(target.errorString());
            src.close();
            QSqlDatabase::removeDatabase(srcConn);
            return stats;
        }

        // Total row estimate for progress reporting.
        qint64 totalRows = 0, doneRows = 0;
        {
            QSqlQuery count(src);
            count.exec(QStringLiteral("SELECT COUNT(*) FROM players"));
            if (count.next())
                totalRows += count.value(0).toLongLong();
            count.exec(QStringLiteral("SELECT COUNT(*) FROM dwrs_ratings"));
            if (count.next())
                totalRows += count.value(0).toLongLong();
        }
        const auto report = [&] {
            if (progress)
                progress(doneRows, totalRows);
        };
        const auto cancelled = [&] { return shouldContinue && !shouldContinue(); };

        // --- 1. Players ---
        QHash<QString, int> idByUid;
        {
            QSqlQuery query(src);
            query.setForwardOnly(true);
            if (!query.exec(QStringLiteral("SELECT * FROM players"))) {
                stats.error = query.lastError().text();
                return stats;
            }
            const QSqlRecord record = query.record();
            const auto col = [&](const QString &name) { return record.indexOf(name); };

            const int cUid = col(QStringLiteral("Unique ID"));
            if (cUid < 0) {
                stats.error = QStringLiteral("Legacy players table has no 'Unique ID' column.");
                return stats;
            }
            const int cName = col(QStringLiteral("Name")), cAge = col(QStringLiteral("Age")),
                      cClub = col(QStringLiteral("Club")),
                      cNat = col(QStringLiteral("Nationality")),
                      cNat2 = col(QStringLiteral("Second Nationality")),
                      cPos = col(QStringLiteral("Position")),
                      cPers = col(QStringLiteral("Personality")),
                      cMedia = col(QStringLiteral("Media Handling")),
                      cApt = col(QStringLiteral("Agreed Playing Time")),
                      cWage = col(QStringLiteral("Wage")),
                      cTv = col(QStringLiteral("Transfer Value")),
                      cAvr = col(QStringLiteral("Average Rating")),
                      cHeight = col(QStringLiteral("Height")),
                      cLf = col(QStringLiteral("Left Foot")),
                      cRf = col(QStringLiteral("Right Foot")),
                      cPf = col(QStringLiteral("Preferred Foot")),
                      cSide = col(QStringLiteral("preferred_side")),
                      cPrim = col(QStringLiteral("primary_role")),
                      cNatPos = col(QStringLiteral("natural_positions")),
                      cRoles = col(QStringLiteral("Assigned Roles")),
                      cTrStatus = col(QStringLiteral("transfer_status")),
                      cLoStatus = col(QStringLiteral("loan_status"));

            std::array<int, kAttrCount> attrCols{};
            for (int i = 0; i < kAttrCount; ++i)
                attrCols[i] = col(attrNames()[i]);

            std::vector<Player> batch;
            batch.reserve(kBatchSize);

            const auto flush = [&]() -> bool {
                if (batch.empty())
                    return true;
                if (!target.upsertPlayers(batch)) {
                    stats.error = target.errorString();
                    return false;
                }
                for (const Player &p : batch)
                    idByUid.insert(p.uid, p.id);
                stats.playersMigrated += static_cast<int>(batch.size());
                doneRows += static_cast<qint64>(batch.size());
                batch.clear();
                report();
                return true;
            };

            while (query.next()) {
                if (cancelled()) {
                    stats.error = QStringLiteral("Cancelled.");
                    return stats;
                }
                Player p;
                p.uid = query.value(cUid).toString();
                if (p.uid.isEmpty()) {
                    note(stats, QStringLiteral("Skipped player row with empty Unique ID"));
                    continue;
                }
                p.name = cName < 0 ? QString() : query.value(cName).toString();
                p.age = cAge < 0 ? 0 : query.value(cAge).toString().toInt();
                p.club = cClub < 0 ? QString() : query.value(cClub).toString();
                p.nationality = cNat < 0 ? QString() : query.value(cNat).toString();
                p.secondNationality = cNat2 < 0 ? QString() : query.value(cNat2).toString();
                p.positionRaw = cPos < 0 ? QString() : query.value(cPos).toString();
                p.personality = cPers < 0 ? QString() : query.value(cPers).toString();
                p.mediaHandling = cMedia < 0 ? QString() : query.value(cMedia).toString();
                p.agreedPlayingTime = cApt < 0 ? QString() : query.value(cApt).toString();
                p.wageRaw = cWage < 0 ? QString() : query.value(cWage).toString();
                p.transferValueRaw = cTv < 0 ? QString() : query.value(cTv).toString();
                p.transferValue = valueToFloat(p.transferValueRaw);
                if (cAvr >= 0) {
                    bool ok = false;
                    const double v = query.value(cAvr).toString().toDouble(&ok);
                    p.averageRating = ok ? v : 0.0;
                }
                p.heightRaw = cHeight < 0 ? QString() : query.value(cHeight).toString();
                p.heightCm = parseHeightCm(p.heightRaw);
                p.leftFoot = cLf < 0 ? QString() : query.value(cLf).toString();
                p.rightFoot = cRf < 0 ? QString() : query.value(cRf).toString();
                p.preferredFoot = cPf < 0 ? QString() : query.value(cPf).toString();

                if (cSide >= 0) {
                    const QString side = query.value(cSide).toString();
                    if (side == QLatin1String("Left") || side == QLatin1String("Right")) {
                        p.preferredSide = side;
                    } else if (!side.isEmpty() && side != QLatin1String("None")) {
                        note(stats, QStringLiteral("Player %1: cleared invalid preferred_side '%2'")
                                        .arg(p.uid, side));
                    }
                }

                p.primaryRole = cPrim < 0 ? QString() : query.value(cPrim).toString();
                if (cNatPos >= 0)
                    p.naturalPositions = parseLegacyList(query.value(cNatPos).toString());
                if (cRoles >= 0) {
                    const QString rolesRaw = query.value(cRoles).toString();
                    p.assignedRoles = parseLegacyList(rolesRaw);
                    if (p.assignedRoles.isEmpty() && !rolesRaw.isEmpty()
                        && rolesRaw != QLatin1String("[]")) {
                        note(stats, QStringLiteral("Player %1: unparseable Assigned Roles '%2'")
                                        .arg(p.uid, rolesRaw.left(60)));
                    }
                }
                p.transferStatus = cTrStatus >= 0 && query.value(cTrStatus).toInt() != 0;
                p.loanStatus = cLoStatus >= 0 && query.value(cLoStatus).toInt() != 0;

                for (int i = 0; i < kAttrCount; ++i) {
                    if (attrCols[i] < 0)
                        continue;
                    const QString raw = query.value(attrCols[i]).toString();
                    int lo = 0, hi = 0;
                    if (!parseAttrValue(raw, &lo, &hi)) {
                        note(stats, QStringLiteral("Player %1: unparseable %2 value '%3'")
                                        .arg(p.uid, attrNames()[i], raw));
                    }
                    p.attrLo[i] = static_cast<uint8_t>(lo);
                    p.attrHi[i] = static_cast<uint8_t>(hi);
                }

                batch.push_back(std::move(p));
                if (static_cast<int>(batch.size()) >= kBatchSize && !flush())
                    return stats;
            }
            if (!flush())
                return stats;
        }

        // --- 2. DWRS history ---
        {
            QSqlQuery query(src);
            query.setForwardOnly(true);
            if (query.exec(QStringLiteral(
                    "SELECT unique_id, role, dwrs_absolute, dwrs_normalized, timestamp "
                    "FROM dwrs_ratings"))) {
                std::vector<DwrsEntry> batch;
                batch.reserve(kBatchSize);
                const auto flush = [&]() -> bool {
                    if (batch.empty())
                        return true;
                    if (!target.appendDwrsRatings(batch)) {
                        stats.error = target.errorString();
                        return false;
                    }
                    stats.ratingsMigrated += static_cast<int>(batch.size());
                    doneRows += static_cast<qint64>(batch.size());
                    batch.clear();
                    report();
                    return true;
                };
                while (query.next()) {
                    if (cancelled()) {
                        stats.error = QStringLiteral("Cancelled.");
                        return stats;
                    }
                    const QString uid = query.value(0).toString();
                    const auto it = idByUid.constFind(uid);
                    if (it == idByUid.constEnd()) {
                        ++stats.orphanedRatings;
                        ++doneRows;
                        continue;
                    }
                    DwrsEntry entry;
                    entry.playerId = it.value();
                    entry.role = query.value(1).toString();
                    entry.absolute = query.value(2).toDouble();
                    entry.normalized = parseNormalized(query.value(3).toString());
                    entry.timestamp = query.value(4).toString();
                    batch.push_back(std::move(entry));
                    if (static_cast<int>(batch.size()) >= kBatchSize && !flush())
                        return stats;
                }
                if (!flush())
                    return stats;
            }
        }

        // --- 3. Settings ---
        {
            QSqlQuery query(src);
            if (query.exec(QStringLiteral("SELECT key, value FROM settings"))) {
                while (query.next()) {
                    const QString key = query.value(0).toString();
                    if (key == QLatin1String("newgen_merge_v1_complete"))
                        continue; // legacy migration marker, not needed
                    target.setSetting(key, query.value(1).toString());
                    ++stats.settingsMigrated;
                }
            }
        }

        // --- 4. National squad & shortlist ---
        const auto migrateIdTable = [&](const QString &table, int *migrated) {
            QSqlQuery query(src);
            if (!query.exec(QStringLiteral("SELECT player_unique_id FROM %1").arg(table)))
                return QList<int>();
            QList<int> ids;
            while (query.next()) {
                const QString uid = query.value(0).toString();
                const auto it = idByUid.constFind(uid);
                if (it == idByUid.constEnd()) {
                    ++stats.orphanedSquadEntries;
                    continue;
                }
                ids.append(it.value());
                ++(*migrated);
            }
            return ids;
        };
        target.setNationalSquadIds(
            migrateIdTable(QStringLiteral("national_squad"), &stats.nationalSquadMigrated));
        target.setShortlistIds(
            migrateIdTable(QStringLiteral("shortlist"), &stats.shortlistMigrated));

        src.close();
        target.close();
    }
    QSqlDatabase::removeDatabase(srcConn);

    // --- 5. Atomic move into place ---
    QFile::remove(targetDbPath);
    if (!QFile::rename(tempPath, targetDbPath)) {
        stats.error = QStringLiteral("Could not move %1 to %2").arg(tempPath, targetDbPath);
        return stats;
    }

    stats.success = true;
    return stats;
}

} // namespace fm
