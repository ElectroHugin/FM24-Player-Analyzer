// Golden-master comparison: computes DWRS + talent in C++ from a migrated DB
// and compares against the legacy Python exporter's CSVs.
//
// Usage: fmgolden <migrated.db> <config.ini> <definitions.json> <dwrs.csv> <talent.csv>
//
// The config.ini is copied to a temp location first so the original is never
// modified (AppConfig seeds missing sections on open).

#include "core/AppConfig.h"
#include "core/Database.h"
#include "core/Definitions.h"
#include "core/DwrsEngine.h"
#include "core/TalentEngine.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>
#include <cstdio>

using namespace fm;

namespace {

struct CsvRow {
    QString uid;
    QString role; // or age_cap for talent
    double a = 0.0;
    double b = 0.0;
};

std::vector<CsvRow> readCsv(const QString &path, bool *ok)
{
    std::vector<CsvRow> rows;
    QFile file(path);
    *ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!*ok)
        return rows;
    QTextStream in(&file);
    in.readLine(); // header
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty())
            continue;
        const QStringList parts = line.split(QLatin1Char(','));
        if (parts.size() != 4)
            continue;
        rows.push_back({parts[0], parts[1], parts[2].toDouble(), parts[3].toDouble()});
    }
    return rows;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 6) {
        std::fprintf(stderr,
                     "Usage: fmgolden <migrated.db> <config.ini> <definitions.json> "
                     "<dwrs.csv> <talent.csv>\n");
        return 2;
    }

    QTemporaryDir tempDir;
    const QString configCopy = tempDir.filePath(QStringLiteral("config.ini"));
    if (!QFile::copy(args[2], configCopy)) {
        std::fprintf(stderr, "Cannot copy config.ini\n");
        return 1;
    }

    Definitions definitions;
    if (!definitions.load(args[3])) {
        std::fprintf(stderr, "definitions: %s\n", qUtf8Printable(definitions.errorString()));
        return 1;
    }
    AppConfig config(configCopy);

    Database db(QStringLiteral("golden"));
    if (!db.open(args[1])) {
        std::fprintf(stderr, "db: %s\n", qUtf8Printable(db.errorString()));
        return 1;
    }

    QElapsedTimer timer;
    timer.start();
    const std::vector<Player> players = db.loadPlayers();
    std::printf("Geladen: %zu Spieler in %.2fs\n", players.size(), timer.elapsed() / 1000.0);

    QHash<QString, int> rowByUid;
    for (int i = 0; i < static_cast<int>(players.size()); ++i)
        rowByUid.insert(players[i].uid, i);

    DwrsEngine engine(definitions, config);

    timer.restart();
    const auto batch = engine.calculateAllAssigned(players, definitions.validRoles());
    const double calcSeconds = timer.elapsed() / 1000.0;
    qint64 totalRatings = 0;
    for (auto it = batch.roleRows.constBegin(); it != batch.roleRows.constEnd(); ++it)
        totalRatings += it.value().size();
    std::printf("DWRS berechnet: %lld Ratings in %.2fs\n",
                static_cast<long long>(totalRatings), calcSeconds);

    // Index computed results: (uid, role) -> (absolute, normalized).
    QHash<QPair<QString, QString>, QPair<double, double>> computed;
    computed.reserve(static_cast<int>(totalRatings));
    QHash<QString, double> bestNormalized;
    for (auto it = batch.roleRows.constBegin(); it != batch.roleRows.constEnd(); ++it) {
        const auto &result = batch.roleResults[it.key()];
        for (size_t j = 0; j < it.value().size(); ++j) {
            const QString &uid = players[it.value()[j]].uid;
            computed.insert({uid, it.key()}, {result.absolute[j], result.normalized[j]});
            if (result.normalized[j] > bestNormalized.value(uid, 0.0))
                bestNormalized.insert(uid, result.normalized[j]);
        }
    }

    // --- Compare DWRS ---
    bool ok = false;
    const auto dwrsGolden = readCsv(args[4], &ok);
    if (!ok) {
        std::fprintf(stderr, "Cannot read %s\n", qUtf8Printable(args[4]));
        return 1;
    }
    qint64 mismatches = 0, missing = 0;
    double maxAbsDev = 0.0, maxNormDev = 0.0;
    for (const CsvRow &row : dwrsGolden) {
        const auto it = computed.constFind({row.uid, row.role});
        if (it == computed.constEnd()) {
            if (++missing <= 5)
                std::fprintf(stderr, "  fehlt: %s / %s\n", qUtf8Printable(row.uid),
                             qUtf8Printable(row.role));
            continue;
        }
        const double absDev =
            std::abs(it.value().first - row.a) / std::max(1.0, std::abs(row.a));
        const double normDev = std::abs(it.value().second - row.b);
        maxAbsDev = std::max(maxAbsDev, absDev);
        maxNormDev = std::max(maxNormDev, normDev);
        if (absDev > 1e-6 || normDev > 0.0) {
            if (++mismatches <= 10)
                std::fprintf(stderr,
                             "  DIFF %s/%s: abs %.9f vs %.9f, norm %.1f vs %.1f\n",
                             qUtf8Printable(row.uid), qUtf8Printable(row.role),
                             it.value().first, row.a, it.value().second, row.b);
        }
    }
    std::printf("DWRS-Vergleich: %zu golden, %lld fehlend, %lld abweichend "
                "(max rel. Abw. absolute %.3e, max Abw. normalized %.3f)\n",
                dwrsGolden.size(), static_cast<long long>(missing),
                static_cast<long long>(mismatches), maxAbsDev, maxNormDev);
    if (static_cast<qint64>(dwrsGolden.size()) != totalRatings)
        std::printf("  Hinweis: C++ hat %lld Ratings, golden %zu\n",
                    static_cast<long long>(totalRatings), dwrsGolden.size());

    // --- Compare Talent ---
    const auto talentGolden = readCsv(args[5], &ok);
    if (!ok) {
        std::fprintf(stderr, "Cannot read %s\n", qUtf8Printable(args[5]));
        return 1;
    }
    qint64 talentMismatches = 0;
    double maxTalentDev = 0.0;
    for (const CsvRow &row : talentGolden) {
        const int playerRow = rowByUid.value(row.uid, -1);
        if (playerRow < 0)
            continue;
        const double talent = TalentEngine::talentForPlayer(
            definitions, players[playerRow], bestNormalized.value(row.uid, 0.0),
            row.role.toDouble() /* age_cap column */);
        const double dev = std::abs(talent - row.b);
        maxTalentDev = std::max(maxTalentDev, dev);
        if (dev > 1e-6) {
            if (++talentMismatches <= 10)
                std::fprintf(stderr, "  TALENT DIFF %s: %.6f vs %.6f\n",
                             qUtf8Printable(row.uid), talent, row.b);
        }
    }
    std::printf("Talent-Vergleich: %zu golden, %lld abweichend (max Abw. %.3e)\n",
                talentGolden.size(), static_cast<long long>(talentMismatches), maxTalentDev);

    const bool pass = mismatches == 0 && missing == 0 && talentMismatches == 0;
    std::printf(pass ? "GOLDEN-GATE: BESTANDEN\n" : "GOLDEN-GATE: FEHLGESCHLAGEN\n");
    return pass ? 0 : 1;
}
