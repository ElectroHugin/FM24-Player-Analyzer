#include <QtTest>

#include <QElapsedTimer>

#include "core/Database.h"
#include "core/Definitions.h"
#include "core/HtmlImporter.h"
#include "core/RoleAssignment.h"
#include "core/Utils.h"

using namespace fm;

namespace {

QString definitionsPath()
{
    return QStringLiteral(LEGACY_DIR) + QStringLiteral("/config/definitions.json");
}

// Minimal FM-style export: the required UID column plus enough columns to
// pass the >= 10 columns check.
QString htmlExport(const QStringList &extraRows)
{
    QString html = QStringLiteral(
        "<html><body><table border=\"1\">\n"
        "<tr><th>UID</th><th>Name</th><th>Age</th><th>Club</th><th>Position</th>"
        "<th>Personality</th><th>Transfer Value</th><th>Height</th><th>Acc</th>"
        "<th>Pac</th><th>Fin</th></tr>\n");
    for (const QString &row : extraRows)
        html += row + QLatin1Char('\n');
    html += QStringLiteral("</table></body></html>");
    return html;
}

QString playerRow(const QString &uid, const QString &name, const QString &age,
                  const QString &club, const QString &value = QStringLiteral("€1.2M"),
                  const QString &acc = QStringLiteral("14"))
{
    return QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
                          "<td>ST (C)</td><td>Model Citizen</td><td>%5</td>"
                          "<td>181 cm</td><td>%6</td><td>15</td><td>12-15</td></tr>")
        .arg(uid, name, age, club, value, acc);
}

} // namespace

class TestHtmlImporter : public QObject
{
    Q_OBJECT

private slots:
    void extractTableBasics()
    {
        HtmlTable table;
        QString error;
        QVERIFY(HtmlImporter::extractTable(
            QStringLiteral("<table><tr><th>A</th><th>B</th></tr>"
                           "<tr><td> x &amp; y </td><td>2</td></tr>"
                           "<tr><td>only-one-cell</td></tr></table>"),
            &table, &error));
        QCOMPARE(table.headers, (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
        QCOMPARE(table.rows.size(), 1);
        QCOMPARE(table.rows.at(0).at(0), QStringLiteral("x & y"));
        QCOMPARE(table.malformedRows, 1);

        QVERIFY(!HtmlImporter::extractTable(QStringLiteral("<p>no table</p>"), &table, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!HtmlImporter::extractTable(
            QStringLiteral("<table><tr><td>no header</td></tr></table>"), &table, &error));
    }

    void extractTableEntitiesAndNesting()
    {
        HtmlTable table;
        QVERIFY(HtmlImporter::extractTable(
            QStringLiteral("<TABLE><TR><TH>Name</TH></TR>"
                           "<TR><TD><b>K&#246;ln</b> &quot;X&quot;</TD></TR></TABLE>"),
            &table));
        QCOMPARE(table.rows.at(0).at(0), QStringLiteral("Köln \"X\""));
    }

    // Regression guard for the O(n²) parser blow-up: a real FM "all players"
    // export has one <th> header row followed by tens of thousands of <td>-only
    // rows. When the tag search was not bounded to the current row, every data
    // cell scanned to end-of-file looking for the next <th> (which never comes
    // again), turning a large export into a parse that never appears to finish.
    // With the row-bounded search this stays linear and returns in well under
    // the (deliberately generous) time budget below.
    void extractTableLargeExportIsLinear()
    {
        const int rows = 20000;
        const int cols = 12;

        QString html;
        html.reserve(rows * cols * 8);
        html += QStringLiteral("<html><body><table border=\"1\">\n<tr>");
        html += QStringLiteral("<th>UID</th>");
        for (int c = 1; c < cols; ++c)
            html += QStringLiteral("<th>C%1</th>").arg(c);
        html += QStringLiteral("</tr>\n");
        for (int r = 0; r < rows; ++r) {
            html += QStringLiteral("<tr>");
            html += QStringLiteral("<td>%1</td>").arg(r + 1);
            for (int c = 1; c < cols; ++c)
                html += QStringLiteral("<td>%1</td>").arg(c);
            html += QStringLiteral("</tr>\n");
        }
        html += QStringLiteral("</table></body></html>");

        HtmlTable table;
        QString error;
        QElapsedTimer timer;
        timer.start();
        QVERIFY2(HtmlImporter::extractTable(html, &table, &error), qPrintable(error));
        const qint64 elapsedMs = timer.elapsed();

        QCOMPARE(table.headers.size(), cols);
        QCOMPARE(table.rows.size(), rows);
        QCOMPARE(table.malformedRows, 0);
        QCOMPARE(table.rows.last().at(0), QString::number(rows));
        // The bounded parser handles this in a few milliseconds; the unbounded
        // version took minutes. 10 s leaves ample slack for slow CI runners.
        QVERIFY2(elapsedMs < 10000,
                 qPrintable(QStringLiteral("parse took %1 ms").arg(elapsedMs)));
    }

    void importNewAndUpdate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Database db(QStringLiteral("import_test_1"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        // First import: two new players.
        const QString html = htmlExport({
            playerRow(QStringLiteral("1001"), QStringLiteral("Alpha Tester"),
                      QStringLiteral("24"), QStringLiteral("FC Test")),
            playerRow(QStringLiteral("r-2002"), QStringLiteral("Beta Newgen"),
                      QStringLiteral("17"), QStringLiteral("FC Test"),
                      QStringLiteral("Not for Sale")),
        });
        ImportResult result = HtmlImporter::importHtml(html, db, db.loadPlayers());
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.playersImported, 2);
        QCOMPARE(result.newPlayers, 2);
        QCOMPARE(result.affectedUids,
                 (QStringList{QStringLiteral("1001"), QStringLiteral("r-2002")}));
        QVERIFY(result.unknownColumns.isEmpty());

        auto players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 2);
        const auto byUid = [&players](const QString &uid) -> Player & {
            for (auto &p : players)
                if (p.uid == uid)
                    return p;
            static Player none;
            return none;
        };
        QCOMPARE(byUid(QStringLiteral("1001")).name, QStringLiteral("Alpha Tester"));
        QCOMPARE(byUid(QStringLiteral("1001")).age, 24);
        QCOMPARE(byUid(QStringLiteral("1001")).heightCm, 181);
        QCOMPARE(byUid(QStringLiteral("1001")).attrLo[idx(Attr::Acceleration)], uint8_t(14));
        QCOMPARE(byUid(QStringLiteral("1001")).attrLo[idx(Attr::Finishing)], uint8_t(12));
        QCOMPARE(byUid(QStringLiteral("1001")).attrHi[idx(Attr::Finishing)], uint8_t(15));
        QCOMPARE(byUid(QStringLiteral("r-2002")).transferValue, kUnbuyableValue);

        // Assign app-managed data, then re-import: it must survive.
        Player &alpha = byUid(QStringLiteral("1001"));
        alpha.assignedRoles = {QStringLiteral("AF-A")};
        alpha.primaryRole = QStringLiteral("AF-A");
        {
            std::vector<Player> batch{alpha};
            QVERIFY(db.upsertPlayers(batch));
        }

        const QString update = htmlExport({
            playerRow(QStringLiteral("1001"), QStringLiteral("Alpha Tester"),
                      QStringLiteral("25"), QStringLiteral("FC Test"),
                      QStringLiteral("€2.5M"), QStringLiteral("15")),
        });
        result = HtmlImporter::importHtml(update, db, db.loadPlayers());
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.newPlayers, 0);

        players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 2);
        const Player &updated = byUid(QStringLiteral("1001"));
        QCOMPARE(updated.age, 25);
        QCOMPARE(updated.attrLo[idx(Attr::Acceleration)], uint8_t(15));
        QCOMPARE(updated.assignedRoles, QStringList{QStringLiteral("AF-A")});
        QCOMPARE(updated.primaryRole, QStringLiteral("AF-A"));
    }

    void uidSanityChecks()
    {
        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_2"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        const QString html = htmlExport({
            playerRow(QString(), QStringLiteral("No Uid"), QStringLiteral("20"),
                      QStringLiteral("FC Test")),
            playerRow(QStringLiteral("5"), QStringLiteral("Twin One"), QStringLiteral("20"),
                      QStringLiteral("FC Test")),
            playerRow(QStringLiteral("5"), QStringLiteral("Twin Two"), QStringLiteral("21"),
                      QStringLiteral("FC Test")),
        });
        const ImportResult result = HtmlImporter::importHtml(html, db, {});
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.emptyUidRows, 1);
        QCOMPARE(result.playersImported, 1);
        QVERIFY(!result.duplicateUidNames.isEmpty());

        const auto players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 1);
        // Duplicate UID: the LAST row wins (legacy keep='last').
        QCOMPARE(players[0].name, QStringLiteral("Twin Two"));
        QCOMPARE(players[0].age, 21);
    }

    void unknownColumnWarning()
    {
        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_3"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        const QString html = QStringLiteral(
            "<table><tr><th>UID</th><th>Name</th><th>Age</th><th>Club</th>"
            "<th>Position</th><th>Personality</th><th>Transfer Value</th>"
            "<th>Height</th><th>Acc</th><th>Mystery</th><th>CON</th></tr>"
            "<tr><td>1</td><td>X</td><td>20</td><td>C</td><td>ST (C)</td>"
            "<td>Balanced</td><td>€1M</td><td>180 cm</td><td>10</td><td>?</td>"
            "<td>ok</td></tr></table>");
        const ImportResult result = HtmlImporter::importHtml(html, db, {});
        QVERIFY2(result.success, qPrintable(result.error));
        // "Mystery" is unknown; "CON" is on the ignore list.
        QCOMPARE(result.unknownColumns, QStringList{QStringLiteral("Mystery")});
    }

    void unificationMissingPrefix()
    {
        // Scenario 1: DB knows r-77; the file exports the bare numeric 77.
        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_4"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        std::vector<Player> seed(1);
        seed[0].uid = QStringLiteral("r-77");
        seed[0].name = QStringLiteral("Regen Man");
        seed[0].assignedRoles = {QStringLiteral("AF-A")};
        QVERIFY(db.upsertPlayers(seed));

        const QString html = htmlExport({
            playerRow(QStringLiteral("77"), QStringLiteral("Regen Man"),
                      QStringLiteral("19"), QStringLiteral("FC Test")),
        });
        const ImportResult result = HtmlImporter::importHtml(html, db, db.loadPlayers());
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.affectedUids, QStringList{QStringLiteral("r-77")});
        QCOMPARE(result.newPlayers, 0);

        const auto players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 1);
        QCOMPARE(players[0].uid, QStringLiteral("r-77"));
        QCOMPARE(players[0].age, 19);
        QCOMPARE(players[0].assignedRoles, QStringList{QStringLiteral("AF-A")});
    }

    void unificationNameConflictLeavesRecords()
    {
        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_5"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        std::vector<Player> seed(1);
        seed[0].uid = QStringLiteral("r-88");
        seed[0].name = QStringLiteral("Someone Else");
        QVERIFY(db.upsertPlayers(seed));

        const QString html = htmlExport({
            playerRow(QStringLiteral("88"), QStringLiteral("Different Name"),
                      QStringLiteral("19"), QStringLiteral("FC Test")),
        });
        const ImportResult result = HtmlImporter::importHtml(html, db, db.loadPlayers());
        QVERIFY2(result.success, qPrintable(result.error));
        QVERIFY(!result.idNameConflicts.isEmpty());
        // Both records exist: the numeric row imported as-is next to r-88.
        QCOMPARE(static_cast<int>(db.loadPlayers().size()), 2);
    }

    void unificationCorruptedNumericRecord()
    {
        // Scenario 2: DB has a corrupted numeric record; the file has r-99.
        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_6"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        std::vector<Player> seed(1);
        seed[0].uid = QStringLiteral("99");
        seed[0].name = QStringLiteral("Corrupt Regen");
        seed[0].assignedRoles = {QStringLiteral("W-S")};
        QVERIFY(db.upsertPlayers(seed));
        // History that must survive the rename.
        std::vector<DwrsEntry> history{{seed[0].id, QStringLiteral("W-S"), 500.0, 60.0,
                                        QStringLiteral("2025-01-01 10:00:00")}};
        QVERIFY(db.appendDwrsRatings(history));

        const QString html = htmlExport({
            playerRow(QStringLiteral("r-99"), QStringLiteral("Corrupt Regen"),
                      QStringLiteral("21"), QStringLiteral("FC Test")),
        });
        const ImportResult result = HtmlImporter::importHtml(html, db, db.loadPlayers());
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.newPlayers, 0);

        const auto players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 1);
        QCOMPARE(players[0].uid, QStringLiteral("r-99"));
        QCOMPARE(players[0].age, 21);
        QCOMPARE(players[0].assignedRoles, QStringList{QStringLiteral("W-S")});
        // History moved with the record.
        const auto ratings = db.latestDwrsRatings();
        QCOMPARE(ratings.value({players[0].id, QStringLiteral("W-S")}).second, 60.0);
    }

    void unificationMergesDuplicates()
    {
        // Scenario 2 with BOTH records present: numeric duplicate merged into r-.
        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_7"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        std::vector<Player> seed(2);
        seed[0].uid = QStringLiteral("55");
        seed[0].name = QStringLiteral("Dup Regen");
        seed[0].assignedRoles = {QStringLiteral("W-S")};
        seed[1].uid = QStringLiteral("r-55");
        seed[1].name = QStringLiteral("Dup Regen");
        QVERIFY(db.upsertPlayers(seed));
        std::vector<DwrsEntry> history{{seed[0].id, QStringLiteral("W-S"), 500.0, 60.0,
                                        QStringLiteral("2025-01-01 10:00:00")}};
        QVERIFY(db.appendDwrsRatings(history));

        const QString html = htmlExport({
            playerRow(QStringLiteral("r-55"), QStringLiteral("Dup Regen"),
                      QStringLiteral("22"), QStringLiteral("FC Test")),
        });
        const ImportResult result = HtmlImporter::importHtml(html, db, db.loadPlayers());
        QVERIFY2(result.success, qPrintable(result.error));

        const auto players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 1);
        QCOMPARE(players[0].uid, QStringLiteral("r-55"));
        // Roles folded in from the merged-away numeric record (fill-empty).
        QCOMPARE(players[0].assignedRoles, QStringList{QStringLiteral("W-S")});
        // History reassigned to the surviving record.
        const auto ratings = db.latestDwrsRatings();
        QCOMPARE(ratings.value({players[0].id, QStringLiteral("W-S")}).second, 60.0);
    }

    void forceUpdateSinglePlayer()
    {
        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_8"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        std::vector<Player> seed(1);
        seed[0].uid = QStringLiteral("r-1");
        seed[0].name = QStringLiteral("Target Man");
        seed[0].assignedRoles = {QStringLiteral("AF-A")};
        QVERIFY(db.upsertPlayers(seed));

        const QString html = htmlExport({
            playerRow(QStringLiteral("424242"), QStringLiteral("Target Man"),
                      QStringLiteral("28"), QStringLiteral("FC Test")),
        });
        QString error;
        const QString name = HtmlImporter::forceUpdateSinglePlayer(
            html, db, db.loadPlayers(), QStringLiteral("r-1"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(name, QStringLiteral("Target Man"));

        const auto players = db.loadPlayers();
        QCOMPARE(static_cast<int>(players.size()), 1);
        QCOMPARE(players[0].uid, QStringLiteral("r-1")); // file UID ignored
        QCOMPARE(players[0].age, 28);
        QCOMPARE(players[0].assignedRoles, QStringList{QStringLiteral("AF-A")});

        // Multi-row files are rejected.
        const QString twoRows = htmlExport({
            playerRow(QStringLiteral("1"), QStringLiteral("A"), QStringLiteral("20"),
                      QStringLiteral("C")),
            playerRow(QStringLiteral("2"), QStringLiteral("B"), QStringLiteral("20"),
                      QStringLiteral("C")),
        });
        HtmlImporter::forceUpdateSinglePlayer(twoRows, db, db.loadPlayers(),
                                              QStringLiteral("r-1"), &error);
        QVERIFY(!error.isEmpty());
    }

    void autoAssignRoles()
    {
        Definitions definitions;
        QVERIFY2(definitions.load(definitionsPath()),
                 qPrintable(definitions.errorString()));

        QTemporaryDir dir;
        Database db(QStringLiteral("import_test_9"));
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.db"))));

        std::vector<Player> players(2);
        players[0].uid = QStringLiteral("1");
        players[0].name = QStringLiteral("Unassigned Striker");
        players[0].positionRaw = QStringLiteral("ST (C)");
        players[1].uid = QStringLiteral("2");
        players[1].name = QStringLiteral("Already Assigned");
        players[1].positionRaw = QStringLiteral("D (C)");
        players[1].assignedRoles = {QStringLiteral("CD-D")};
        QVERIFY(db.upsertPlayers(players));

        QString error;
        const QStringList changed =
            RoleAssignment::autoAssignRolesToUnassigned(db, players, definitions, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(changed, QStringList{QStringLiteral("1")});
        QVERIFY(!players[0].assignedRoles.isEmpty());
        // Persisted, sorted, and derived from the ST (C) role mapping.
        const auto reloaded = db.loadPlayers();
        for (const Player &p : reloaded) {
            if (p.uid == QStringLiteral("1")) {
                QCOMPARE(p.assignedRoles, players[0].assignedRoles);
                QStringList sorted = p.assignedRoles;
                std::sort(sorted.begin(), sorted.end());
                QCOMPARE(p.assignedRoles, sorted);
            } else {
                QCOMPARE(p.assignedRoles, QStringList{QStringLiteral("CD-D")});
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestHtmlImporter)
#include "test_htmlimporter.moc"
