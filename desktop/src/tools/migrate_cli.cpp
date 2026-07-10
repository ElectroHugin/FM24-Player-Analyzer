// Command-line legacy-DB migration: fmmigrate <legacy.db> <target.db>
// Used for batch verification of all legacy databases; the GUI wizard uses
// the same LegacyMigrator.

#include "core/LegacyMigrator.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 3) {
        std::fprintf(stderr, "Usage: fmmigrate <legacy.db> <target.db>\n");
        return 2;
    }

    QElapsedTimer timer;
    timer.start();

    qint64 lastPercent = -1;
    fm::LegacyMigrator migrator;
    const fm::MigrationStats stats = migrator.migrate(
        args[1], args[2],
        [&](qint64 current, qint64 total) {
            const qint64 percent = total > 0 ? current * 100 / total : 0;
            if (percent != lastPercent) {
                std::fprintf(stderr, "\r%lld%% (%lld/%lld)", percent, current, total);
                lastPercent = percent;
            }
        });
    std::fprintf(stderr, "\n");

    if (!stats.success) {
        std::fprintf(stderr, "FEHLER: %s\n", qUtf8Printable(stats.error));
        return 1;
    }

    std::printf("OK in %.1fs\n", timer.elapsed() / 1000.0);
    std::printf("  Spieler:        %d\n", stats.playersMigrated);
    std::printf("  DWRS-Historie:  %d (verwaist übersprungen: %d)\n", stats.ratingsMigrated,
                stats.orphanedRatings);
    std::printf("  Settings:       %d\n", stats.settingsMigrated);
    std::printf("  Nationalkader:  %d, Shortlist: %d (verwaist: %d)\n",
                stats.nationalSquadMigrated, stats.shortlistMigrated,
                stats.orphanedSquadEntries);
    if (!stats.coercions.isEmpty()) {
        std::printf("  Bereinigungen:  %lld (erste %d):\n",
                    static_cast<long long>(stats.coercions.size()),
                    static_cast<int>(qMin<qsizetype>(stats.coercions.size(), 10)));
        for (int i = 0; i < stats.coercions.size() && i < 10; ++i)
            std::printf("    - %s\n", qUtf8Printable(stats.coercions[i]));
    }
    return 0;
}
