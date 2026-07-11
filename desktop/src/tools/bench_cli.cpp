// Load-path benchmark: times exactly what AppContext::reloadFromDatabase does
// on app start / after imports (players + latest ratings + uid mapping).
//
// Usage: fmbench <migrated.db>

#include "core/Database.h"
#include "core/PlayerStore.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 2) {
        std::fprintf(stderr, "Usage: fmbench <migrated.db>\n");
        return 2;
    }

    QElapsedTimer timer;
    timer.start();
    fm::Database db(QStringLiteral("bench"));
    if (!db.open(args[1])) {
        std::fprintf(stderr, "FEHLER: %s\n", qUtf8Printable(db.errorString()));
        return 1;
    }
    const qint64 openMs = timer.restart();

    fm::PlayerStore store;
    store.reset(db.loadPlayers());
    const qint64 playersMs = timer.restart();

    const fm::LatestRatings latest = db.latestDwrsRatings();
    const qint64 ratingsMs = timer.restart();

    // uid-keyed role ratings, as built in AppContext::reloadFromDatabase.
    QHash<QString, QHash<QString, double>> roleRatings;
    for (auto it = latest.constBegin(); it != latest.constEnd(); ++it) {
        const int row = store.rowById(it.key().first);
        if (row >= 0)
            roleRatings[it.key().second].insert(store.at(row).uid, it.value().second);
    }
    const qint64 mappingMs = timer.restart();

    std::printf("Spieler geladen:   %d in %lld ms\n", store.size(), playersMs);
    std::printf("Ratings geladen:   %lld in %lld ms\n",
                static_cast<long long>(latest.size()), ratingsMs);
    std::printf("Rollen-Mapping:    %lld Rollen in %lld ms\n",
                static_cast<long long>(roleRatings.size()), mappingMs);
    std::printf("Gesamt (inkl. Open %lld ms): %lld ms\n", openMs,
                openMs + playersMs + ratingsMs + mappingMs);
    return 0;
}
