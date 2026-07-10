// Golden-master comparison part 2: squad building, gap analysis, tactic
// explorer vs the legacy Python exporter (export_golden_squad.py).
//
// Usage: fmsquad <migrated.db> <config.ini> <definitions.json> <squad.json>

#include "core/AppConfig.h"
#include "core/Database.h"
#include "core/Definitions.h"
#include "core/GapAnalysis.h"
#include "core/SquadBuilder.h"
#include "core/TacticExplorer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>

using namespace fm;

namespace {

qint64 g_mismatches = 0;

void diff(const QString &context, const QString &expected, const QString &actual)
{
    if (++g_mismatches <= 25)
        std::fprintf(stderr, "  DIFF %s: golden='%s' cpp='%s'\n", qUtf8Printable(context),
                     qUtf8Printable(expected), qUtf8Printable(actual));
}

// Compare a computed XI against the golden {slot: uid|null} object.
void compareXi(const QString &context, const QHash<QString, XiCell> &xi,
               const QJsonObject &golden)
{
    for (auto it = golden.begin(); it != golden.end(); ++it) {
        const QString expected = it.value().isNull() ? QString() : it.value().toString();
        const QString actual = xi.value(it.key()).playerUid;
        if (expected != actual)
            diff(context + QLatin1Char('/') + it.key(), expected.isEmpty() ? "null" : expected,
                 actual.isEmpty() ? "null" : actual);
    }
}

void compareUidList(const QString &context, const std::vector<const Player *> &players,
                    const QJsonArray &golden)
{
    if (static_cast<int>(players.size()) != golden.size()) {
        diff(context + "/size", QString::number(golden.size()),
             QString::number(players.size()));
        return;
    }
    for (int i = 0; i < golden.size(); ++i) {
        if (players[i]->uid != golden[i].toString())
            diff(QStringLiteral("%1[%2]").arg(context).arg(i), golden[i].toString(),
                 players[i]->uid);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 5) {
        std::fprintf(stderr,
                     "Usage: fmsquad <migrated.db> <config.ini> <definitions.json> <squad.json>\n");
        return 2;
    }

    QTemporaryDir tempDir;
    const QString configCopy = tempDir.filePath(QStringLiteral("config.ini"));
    QFile::copy(args[2], configCopy);

    Definitions definitions;
    if (!definitions.load(args[3])) {
        std::fprintf(stderr, "definitions: %s\n", qUtf8Printable(definitions.errorString()));
        return 1;
    }
    AppConfig config(configCopy);

    Database db(QStringLiteral("squadgolden"));
    if (!db.open(args[1])) {
        std::fprintf(stderr, "db: %s\n", qUtf8Printable(db.errorString()));
        return 1;
    }

    QFile goldenFile(args[4]);
    if (!goldenFile.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "Cannot read %s\n", qUtf8Printable(args[4]));
        return 1;
    }
    const QJsonObject golden = QJsonDocument::fromJson(goldenFile.readAll()).object();

    const std::vector<Player> players = db.loadPlayers();
    QHash<QString, const Player *> playersByUid;
    QHash<int, const Player *> playersById;
    for (const Player &p : players) {
        playersByUid.insert(p.uid, &p);
        playersById.insert(p.id, &p);
    }

    // uid-keyed numeric master ratings from the migrated dwrs history.
    RoleRatings ratings;
    const LatestRatings latest = db.latestDwrsRatings();
    for (auto it = latest.constBegin(); it != latest.constEnd(); ++it) {
        const Player *p = playersById.value(it.key().first);
        if (p)
            ratings[it.key().second].insert(p->uid, it.value().second);
    }

    const QString userClub = db.setting(QStringLiteral("user_club"));
    const QString secondClub = db.setting(QStringLiteral("second_team_club"));
    if (userClub != golden.value(QLatin1String("user_club")).toString())
        diff(QStringLiteral("user_club"), golden.value(QLatin1String("user_club")).toString(),
             userClub);

    std::vector<const Player *> myClub, secondTeam;
    for (const Player &p : players) {
        if (p.club == userClub)
            myClub.push_back(&p);
        else if (!secondClub.isEmpty() && p.club == secondClub)
            secondTeam.push_back(&p);
    }
    std::printf("%zu Spieler geladen, %zu im Club '%s'\n", players.size(), myClub.size(),
                qUtf8Printable(userClub));

    SquadBuilder builder(definitions, config);
    TacticExplorer explorer(definitions, builder);

    const double dispThr = config.gapAnalysisSetting(QStringLiteral("displacement_threshold"));
    const double dropThr = config.gapAnalysisSetting(QStringLiteral("dropoff_threshold"));
    const double sidePen = config.gapAnalysisSetting(QStringLiteral("wrong_side_penalty"));

    QElapsedTimer timer;
    timer.start();

    const QJsonObject goldenTactics = golden.value(QLatin1String("tactics")).toObject();
    int tacticCount = 0;
    for (const QString &tactic : definitions.tacticNamesOrdered()) {
        const QJsonObject g = goldenTactics.value(tactic).toObject();
        if (g.isEmpty())
            continue;
        ++tacticCount;
        const QHash<QString, QString> positions = definitions.tacticRoles().value(tactic);
        const QStringList slotOrder = definitions.tacticSlotOrder(tactic);

        const SquadResult squad =
            builder.calculateSquadAndSurplus(myClub, positions, slotOrder, ratings);
        const DevelopmentSquads dev = builder.calculateDevelopmentSquads(
            secondTeam, squad.depthPool, positions, slotOrder, ratings, squad.depthPlayerUids);

        compareXi(tactic + "/xi", squad.startingXi, g.value(QLatin1String("starting_xi")).toObject());
        compareXi(tactic + "/b", squad.bTeam, g.value(QLatin1String("b_team")).toObject());
        compareXi(tactic + "/youth", dev.youthXi, g.value(QLatin1String("youth_xi")).toObject());
        compareXi(tactic + "/second", dev.secondTeamXi,
                  g.value(QLatin1String("second_xi")).toObject());
        compareUidList(tactic + "/loan", dev.loanCandidates,
                       g.value(QLatin1String("loan")).toArray());
        compareUidList(tactic + "/sell", dev.sellCandidates,
                       g.value(QLatin1String("sell")).toArray());

        // Depth: role -> best pick NAME (legacy cells carry no uid) + uid set.
        const QJsonObject goldenDepth = g.value(QLatin1String("depth")).toObject();
        for (auto it = goldenDepth.begin(); it != goldenDepth.end(); ++it) {
            const QList<DepthOption> options = squad.bestDepthOptions.value(it.key());
            const QString actual = options.isEmpty() ? QString() : options.first().name;
            if (actual != it.value().toString())
                diff(tactic + "/depth/" + it.key(), it.value().toString(), actual);
        }
        QStringList depthUids(squad.depthPlayerUids.begin(), squad.depthPlayerUids.end());
        std::sort(depthUids.begin(), depthUids.end());
        const QJsonArray goldenDepthUids = g.value(QLatin1String("depth_uids")).toArray();
        QStringList goldenDepthList;
        for (const QJsonValue &v : goldenDepthUids)
            goldenDepthList.append(v.toString());
        if (depthUids != goldenDepthList)
            diff(tactic + "/depth_uids", goldenDepthList.join(QLatin1Char(',')),
                 depthUids.join(QLatin1Char(',')));

        // Gap analysis on both teams: ordered [slot, gap_score].
        const auto compareGaps = [&](const QString &key,
                                     const QHash<QString, XiCell> &team) {
            const std::vector<Gap> gaps = GapAnalysis::analyzeTeamGaps(
                team, positions, slotOrder, playersByUid, ratings, dispThr, dropThr, sidePen);
            const QJsonArray goldenGaps = g.value(key).toArray();
            if (static_cast<int>(gaps.size()) != goldenGaps.size()) {
                diff(tactic + "/" + key + "/count", QString::number(goldenGaps.size()),
                     QString::number(gaps.size()));
                return;
            }
            for (int i = 0; i < goldenGaps.size(); ++i) {
                const QJsonArray entry = goldenGaps[i].toArray();
                const QString slot = entry[0].toString();
                const double score = entry[1].toDouble();
                if (gaps[i].slot != slot || std::abs(gaps[i].gapScore - score) > 1e-4) {
                    diff(QStringLiteral("%1/%2[%3]").arg(tactic, key).arg(i),
                         QStringLiteral("%1:%2").arg(slot).arg(score),
                         QStringLiteral("%1:%2").arg(gaps[i].slot).arg(gaps[i].gapScore));
                }
            }
        };
        compareGaps(QStringLiteral("gaps_xi"), squad.startingXi);
        compareGaps(QStringLiteral("gaps_b"), squad.bTeam);
    }

    // Tactic explorer ranking.
    const std::vector<TacticMetrics> ranking = explorer.analyzeAllTactics(myClub, ratings);
    const QJsonArray goldenRanking = golden.value(QLatin1String("explorer")).toArray();
    if (static_cast<int>(ranking.size()) != goldenRanking.size()) {
        diff(QStringLiteral("explorer/count"), QString::number(goldenRanking.size()),
             QString::number(ranking.size()));
    } else {
        for (int i = 0; i < goldenRanking.size(); ++i) {
            const QJsonArray entry = goldenRanking[i].toArray();
            const QString tactic = entry[0].toString();
            const int filled = entry[1].toInt();
            const double med = entry[2].isNull() ? -1.0 : entry[2].toDouble();
            const TacticMetrics &m = ranking[i];
            if (m.tactic != tactic || m.filledSlots != filled
                || std::abs(m.overallMedian - med) > 1e-6) {
                diff(QStringLiteral("explorer[%1]").arg(i),
                     QStringLiteral("%1 f=%2 med=%3").arg(tactic).arg(filled).arg(med),
                     QStringLiteral("%1 f=%2 med=%3")
                         .arg(m.tactic)
                         .arg(m.filledSlots)
                         .arg(m.overallMedian));
            }
        }
    }

    std::printf("%d Taktiken analysiert und verglichen in %.2fs\n", tacticCount,
                timer.elapsed() / 1000.0);
    std::printf("Abweichungen: %lld\n", static_cast<long long>(g_mismatches));
    std::printf(g_mismatches == 0 ? "GOLDEN-GATE 2: BESTANDEN\n"
                                  : "GOLDEN-GATE 2: FEHLGESCHLAGEN\n");
    return g_mismatches == 0 ? 0 : 1;
}
