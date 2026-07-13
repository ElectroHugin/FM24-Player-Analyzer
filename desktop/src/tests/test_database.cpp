#include <QtTest>

#include <QDir>
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
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "test_database.moc"
