#include <QtTest>

#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/Database.h"
#include "core/LegacyMigrator.h"
#include "core/Utils.h"

using namespace fm;

// Builds a minimal legacy-format DB (players all-TEXT, dwrs_ratings keyed by
// uid) and verifies migration into the new typed schema.
class TestMigrator : public QObject
{
    Q_OBJECT

    static void createLegacyFixture(const QString &path)
    {
        const QString conn = QStringLiteral("legacy_fixture");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE players (\"Unique ID\" TEXT PRIMARY KEY, \"Assigned Roles\" TEXT,"
                " Name TEXT, Age TEXT, Club TEXT, Nationality TEXT, \"Second Nationality\" TEXT,"
                " Position TEXT, Personality TEXT, \"Transfer Value\" TEXT, Wage TEXT,"
                " \"Average Rating\" TEXT, Height TEXT, \"Left Foot\" TEXT, \"Right Foot\" TEXT,"
                " \"Preferred Foot\" TEXT, \"Agreed Playing Time\" TEXT, Pace TEXT,"
                " Acceleration TEXT, Finishing TEXT, preferred_side TEXT, primary_role TEXT,"
                " natural_positions TEXT, transfer_status INTEGER, loan_status INTEGER)")));

            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO players VALUES"
                "('1001', '[''CD-D'', ''BPD-D'']', 'Alpha Tester', '24', 'FC Test', 'GER', '',"
                " 'D (C)', 'Model Citizen', '€1.2M', '€5,000 p/w', '7.12', '188 cm', 'Weak',"
                " 'Very Strong', 'Right', 'Star Player', '15', '12-15', '-', 'Left', 'CD-D',"
                " '[''D (C)'']', 1, 0),"
                "('r-2002', '[]', 'Beta Newgen', '17', 'FC Test', 'ESP', 'GER', 'ST (C)',"
                " 'Slack', 'Not for Sale', '€500 p/w', '-', '181 cm', 'Very Strong', 'Weak',"
                " 'Left', '', '18', '17', '16', 'preferred_side', '', '', 0, 0),"
                "('3003', '[''GK-D'']', 'Gamma Keeper', '30', 'Other FC', 'ITA', '', 'GK',"
                " 'Balanced', '€800K - €1.1M', '€9,000 p/w', '6.90', '191 cm', 'Reasonable',"
                " 'Very Strong', 'Right', '', '8', '9', '2', '', 'GK-D', '[]', 0, 1)")));

            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE dwrs_ratings (unique_id TEXT, role TEXT, dwrs_absolute REAL,"
                " dwrs_normalized TEXT, timestamp TEXT,"
                " PRIMARY KEY (unique_id, role, timestamp))")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO dwrs_ratings VALUES"
                "('1001', 'CD-D', 764.9, '68%', '2025-08-31 14:20:49'),"
                "('1001', 'CD-D', 795.0, '71%', '2025-09-30 10:00:00'),"
                "('3003', 'GK-D', 400.0, '55%', '2025-08-31 14:20:49'),"
                "('9999', 'CD-D', 100.0, '20%', '2025-08-31 14:20:49')"))); // orphan

            QVERIFY(q.exec(QStringLiteral("CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT)")));
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO settings VALUES ('user_club', 'FC Test'),"
                " ('newgen_merge_v1_complete', 'true'), ('favorite_tactic_1', '4-4-2 GIN 25')")));

            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE national_squad (player_unique_id TEXT PRIMARY KEY)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO national_squad VALUES ('1001'), ('404')")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE shortlist (player_unique_id TEXT PRIMARY KEY)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO shortlist VALUES ('r-2002')")));
            db.close();
        }
        QSqlDatabase::removeDatabase(conn);
    }

private slots:
    void parseAttrValue()
    {
        int lo = 0, hi = 0;
        QVERIFY(LegacyMigrator::parseAttrValue(QStringLiteral("14"), &lo, &hi));
        QCOMPARE(lo, 14);
        QCOMPARE(hi, 14);
        QVERIFY(LegacyMigrator::parseAttrValue(QStringLiteral("12-15"), &lo, &hi));
        QCOMPARE(lo, 12);
        QCOMPARE(hi, 15);
        QVERIFY(LegacyMigrator::parseAttrValue(QStringLiteral("-"), &lo, &hi));
        QCOMPARE(lo, 0);
        QVERIFY(LegacyMigrator::parseAttrValue(QString(), &lo, &hi));
        QVERIFY(!LegacyMigrator::parseAttrValue(QStringLiteral("abc"), &lo, &hi));
        QVERIFY(!LegacyMigrator::parseAttrValue(QStringLiteral("25"), &lo, &hi));
    }

    void parseLegacyList()
    {
        QCOMPARE(LegacyMigrator::parseLegacyList(QStringLiteral("['CD-D', 'DM-S']")),
                 (QStringList{QStringLiteral("CD-D"), QStringLiteral("DM-S")}));
        QCOMPARE(LegacyMigrator::parseLegacyList(QStringLiteral("[\"A\", 'B']")),
                 (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
        QCOMPARE(LegacyMigrator::parseLegacyList(QStringLiteral("['D (C)']")),
                 QStringList{QStringLiteral("D (C)")});
        // Embedded quote via escape.
        QCOMPARE(LegacyMigrator::parseLegacyList(QStringLiteral("['O\\'Brien']")),
                 QStringList{QStringLiteral("O'Brien")});
        QVERIFY(LegacyMigrator::parseLegacyList(QStringLiteral("[]")).isEmpty());
        QVERIFY(LegacyMigrator::parseLegacyList(QString()).isEmpty());
        QVERIFY(LegacyMigrator::parseLegacyList(QStringLiteral("None")).isEmpty());
    }

    void parseNormalizedAndHeight()
    {
        QCOMPARE(LegacyMigrator::parseNormalized(QStringLiteral("68%")), 68.0);
        QCOMPARE(LegacyMigrator::parseNormalized(QStringLiteral("57.3")), 57.3);
        QCOMPARE(LegacyMigrator::parseNormalized(QString()), 0.0);
        QCOMPARE(LegacyMigrator::parseHeightCm(QStringLiteral("188 cm")), 188);
        QCOMPARE(LegacyMigrator::parseHeightCm(QStringLiteral("-")), 0);
    }

    void fullMigration()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString legacyPath = dir.filePath(QStringLiteral("legacy.db"));
        const QString targetPath = dir.filePath(QStringLiteral("new.db"));
        createLegacyFixture(legacyPath);

        LegacyMigrator migrator;
        const MigrationStats stats = migrator.migrate(legacyPath, targetPath);
        QVERIFY2(stats.success, qPrintable(stats.error));
        QCOMPARE(stats.playersMigrated, 3);
        QCOMPARE(stats.ratingsMigrated, 3);
        QCOMPARE(stats.orphanedRatings, 1);
        QCOMPARE(stats.settingsMigrated, 2); // merge marker skipped
        QCOMPARE(stats.nationalSquadMigrated, 1);
        QCOMPARE(stats.orphanedSquadEntries, 1);
        QCOMPARE(stats.shortlistMigrated, 1);
        QVERIFY(!QFile::exists(targetPath + QStringLiteral(".migrating")));

        // Verify content in the new schema.
        Database db(QStringLiteral("verify_migration"));
        QVERIFY(db.open(targetPath));
        auto players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 3);

        const auto byUid = [&](const QString &uid) -> const Player & {
            for (const auto &p : players)
                if (p.uid == uid)
                    return p;
            static Player none;
            return none;
        };

        const Player &alpha = byUid(QStringLiteral("1001"));
        QCOMPARE(alpha.name, QStringLiteral("Alpha Tester"));
        QCOMPARE(alpha.age, 24);
        QCOMPARE(alpha.transferValue, 1'200'000.0);
        QCOMPARE(alpha.heightCm, 188);
        QCOMPARE(alpha.leftFoot, QStringLiteral("Weak"));
        QCOMPARE(alpha.preferredSide, QStringLiteral("Left"));
        QCOMPARE(alpha.assignedRoles,
                 (QStringList{QStringLiteral("BPD-D"), QStringLiteral("CD-D")})); // sorted by role
        QCOMPARE(alpha.naturalPositions, QStringList{QStringLiteral("D (C)")});
        QVERIFY(alpha.transferStatus);
        QVERIFY(alpha.inNationalSquad);
        const int pace = attrIndexByName(QStringLiteral("Pace"));
        const int accel = attrIndexByName(QStringLiteral("Acceleration"));
        const int finishing = attrIndexByName(QStringLiteral("Finishing"));
        QCOMPARE(static_cast<int>(alpha.attrLo[pace]), 15);
        QCOMPARE(static_cast<int>(alpha.attrLo[accel]), 12);
        QCOMPARE(static_cast<int>(alpha.attrHi[accel]), 15);
        QCOMPARE(alpha.attrMean(accel), 13.5);
        QVERIFY(!alpha.hasAttr(finishing)); // "-" -> missing

        const Player &beta = byUid(QStringLiteral("r-2002"));
        QCOMPARE(beta.transferValue, kUnbuyableValue);
        QCOMPARE(beta.preferredSide, QString()); // garbage value cleared
        QVERIFY(beta.onShortlist);
        QCOMPARE(beta.averageRating, 0.0); // "-"

        const Player &gamma = byUid(QStringLiteral("3003"));
        QVERIFY(gamma.loanStatus);
        QCOMPARE(gamma.transferValue, 800'000.0); // range lower bound

        // DWRS history: latest per (player, role).
        const auto latest = db.latestDwrsRatings();
        QCOMPARE(latest.size(), 2);
        const auto alphaLatest = latest.value({alpha.id, QStringLiteral("CD-D")});
        QCOMPARE(alphaLatest.first, 795.0);
        QCOMPARE(alphaLatest.second, 71.0);

        // Full history for alpha preserved both snapshots.
        const auto history = db.dwrsHistory({alpha.id}, QStringLiteral("CD-D"));
        QCOMPARE(static_cast<int>(history.size()), 2);

        QCOMPARE(db.setting(QStringLiteral("user_club")), QStringLiteral("FC Test"));
        QCOMPARE(db.setting(QStringLiteral("newgen_merge_v1_complete")), QString());
        db.close();
    }

    void migrationIsIdempotentAndNonDestructive()
    {
        QTemporaryDir dir;
        const QString legacyPath = dir.filePath(QStringLiteral("legacy.db"));
        const QString targetPath = dir.filePath(QStringLiteral("new.db"));
        createLegacyFixture(legacyPath);

        const QByteArray before = [&] {
            QFile f(legacyPath);
            f.open(QIODevice::ReadOnly);
            return f.readAll();
        }();

        LegacyMigrator migrator;
        QVERIFY(migrator.migrate(legacyPath, targetPath).success);
        // Second run overwrites the target cleanly.
        QVERIFY(migrator.migrate(legacyPath, targetPath).success);

        const QByteArray after = [&] {
            QFile f(legacyPath);
            f.open(QIODevice::ReadOnly);
            return f.readAll();
        }();
        QCOMPARE(before, after); // legacy DB untouched
    }
};

QTEST_GUILESS_MAIN(TestMigrator)
#include "test_migrator.moc"
