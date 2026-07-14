#include <QtTest>

#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "core/Database.h"
#include "core/Player.h"

using namespace fm;

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    // Regression for the WAL backup bug: a freshly committed player lives in the
    // -wal sidecar, not yet in the main .db file. createBackup must capture it
    // (VACUUM INTO), which a plain file copy of the .db would not.
    void backupIncludesUncheckpointedWalData()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbFile = dir.filePath(QStringLiteral("save.db"));
        const QString backupsDir = dir.filePath(QStringLiteral("backups"));

        {
            Database db(QStringLiteral("test_backup_src"));
            QVERIFY(db.open(dbFile));

            Player player;
            player.uid = QStringLiteral("r-42");
            player.name = QStringLiteral("Müller");
            std::vector<Player> batch{player};
            QVERIFY(db.upsertPlayers(batch));

            // Back up while the source connection is still open (the import
            // scenario: the app holds the live connection during the backup).
            QString error;
            QVERIFY2(Database::createBackup(dbFile, backupsDir, &error), qPrintable(error));
            db.close();
        }

        const QDir bdir(backupsDir);
        const QStringList files = bdir.entryList({QStringLiteral("*.db")}, QDir::Files);
        QCOMPARE(files.size(), 1);

        Database backup(QStringLiteral("test_backup_dst"));
        QVERIFY(backup.open(bdir.filePath(files.first())));
        const std::vector<Player> restored = backup.loadPlayers();
        QCOMPARE(static_cast<int>(restored.size()), 1);
        QCOMPARE(restored[0].uid, QStringLiteral("r-42"));
        QCOMPARE(restored[0].name, QStringLiteral("Müller"));
        backup.close();
    }

    // dwrs_latest must always hold the newest rating per (player, role), and an
    // out-of-order (older) append must never make it go backwards.
    void dwrsLatestTracksNewest()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Database db(QStringLiteral("test_latest"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("save.db"))));

        Player p;
        p.uid = QStringLiteral("p1");
        p.name = QStringLiteral("Test"); // players.name is NOT NULL
        std::vector<Player> batch{p};
        QVERIFY2(db.upsertPlayers(batch), qPrintable(db.errorString()));
        const int id = batch[0].id;

        const auto entry = [id](double normalized, const QString &ts) {
            DwrsEntry e;
            e.playerId = id;
            e.role = QStringLiteral("CD");
            e.absolute = normalized / 5.0;
            e.normalized = normalized;
            e.timestamp = ts;
            return e;
        };

        QVERIFY(db.appendDwrsRatings({entry(50.0, QStringLiteral("2024-01-01 00:00:00"))}));
        QVERIFY(db.appendDwrsRatings({entry(60.0, QStringLiteral("2024-06-01 00:00:00"))}));
        QCOMPARE(db.latestDwrsRatings().value({id, QStringLiteral("CD")}).second, 60.0);

        // An older append is ignored by dwrs_latest.
        QVERIFY(db.appendDwrsRatings({entry(30.0, QStringLiteral("2023-01-01 00:00:00"))}));
        QCOMPARE(db.latestDwrsRatings().value({id, QStringLiteral("CD")}).second, 60.0);
        db.close();
    }

    // Opening a v1 database must materialize dwrs_latest from the existing
    // history and drop the dead registration/information columns.
    void migratesV1ToV2()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbFile = dir.filePath(QStringLiteral("v1.db"));

        // Build a minimal v1-shaped database by hand.
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                         QStringLiteral("v1setup"));
            raw.setDatabaseName(dbFile);
            QVERIFY(raw.open());
            QSqlQuery q(raw);
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE players (id INTEGER PRIMARY KEY, uid TEXT NOT NULL UNIQUE, "
                "name TEXT, registration TEXT, information TEXT)")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE dwrs_history (player_id INTEGER, role TEXT, absolute REAL, "
                "normalized REAL, ts TEXT, PRIMARY KEY (player_id, role, ts))")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO players (id, uid, name) VALUES (1, 'p1', 'Test')")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO dwrs_history VALUES (1, 'CD', 10, 50, '2024-01-01 00:00:00')")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO dwrs_history VALUES (1, 'CD', 12, 60, '2024-06-01 00:00:00')")));
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 1")));
            raw.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("v1setup"));

        // Opening through fm::Database triggers the v1 -> v2 migration.
        Database db(QStringLiteral("test_migrate"));
        QVERIFY2(db.open(dbFile), qPrintable(db.errorString()));

        const LatestRatings latest = db.latestDwrsRatings();
        QCOMPARE(latest.size(), 1);
        QCOMPARE(latest.value({1, QStringLiteral("CD")}).second, 60.0);

        QSqlQuery info(db.handle());
        QVERIFY(info.exec(QStringLiteral("PRAGMA table_info(players)")));
        QStringList cols;
        while (info.next())
            cols << info.value(1).toString();
        QVERIFY(!cols.contains(QStringLiteral("registration")));
        QVERIFY(!cols.contains(QStringLiteral("information")));
        db.close();
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "test_database.moc"
