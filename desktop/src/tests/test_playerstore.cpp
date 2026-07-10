#include <QtTest>

#include "core/Constants.h"
#include "core/PlayerStore.h"

using namespace fm;

class TestPlayerStore : public QObject
{
    Q_OBJECT

private slots:
    void addAndFind()
    {
        PlayerStore store;
        Player p;
        p.id = 7;
        p.uid = QStringLiteral("r-123");
        p.name = QStringLiteral("Test Newgen");
        store.add(p);

        QCOMPARE(store.size(), 1);
        QCOMPARE(store.rowByUid(QStringLiteral("r-123")), 0);
        QCOMPARE(store.rowById(7), 0);
        QVERIFY(store.findByUid(QStringLiteral("r-123")) != nullptr);
        QVERIFY(store.findByUid(QStringLiteral("999")) == nullptr);
    }

    void resetRebuildsIndexes()
    {
        std::vector<Player> players(3);
        players[0].uid = QStringLiteral("1");
        players[1].uid = QStringLiteral("2");
        players[2].uid = QStringLiteral("r-3");

        PlayerStore store;
        store.reset(std::move(players));
        QCOMPARE(store.size(), 3);
        QCOMPARE(store.rowByUid(QStringLiteral("r-3")), 2);
    }

    void removeRows()
    {
        std::vector<Player> players(4);
        for (int i = 0; i < 4; ++i)
            players[i].uid = QString::number(i);
        PlayerStore store;
        store.reset(std::move(players));

        store.removeRows({1, 3});
        QCOMPARE(store.size(), 2);
        QCOMPARE(store.rowByUid(QStringLiteral("0")), 0);
        QCOMPARE(store.rowByUid(QStringLiteral("2")), 1);
        QCOMPARE(store.rowByUid(QStringLiteral("1")), -1);
    }

    void attrMeanHandlesRanges()
    {
        Player p;
        const int pace = attrIndexByName(QStringLiteral("Pace"));
        QVERIFY(pace >= 0);
        p.attrLo[pace] = 12;
        p.attrHi[pace] = 15;
        QCOMPARE(p.attrMean(pace), 13.5); // range "12-15" -> mean
        QVERIFY(p.hasAttr(pace));

        const int flair = attrIndexByName(QStringLiteral("Flair"));
        QVERIFY(!p.hasAttr(flair));
    }

    void parsedPositions()
    {
        Player p;
        p.positionRaw = QStringLiteral("D (RC), DM");
        const auto positions = p.parsedPositions();
        QVERIFY(positions.contains(QStringLiteral("D (R)")));
        QVERIFY(positions.contains(QStringLiteral("D (C)")));
        QVERIFY(positions.contains(QStringLiteral("DM")));
    }

    void attributeMappingComplete()
    {
        // Every rated attribute must be reachable from an FM export header.
        const auto &mapping = attributeMapping();
        int ratedCount = 0;
        for (auto it = mapping.constBegin(); it != mapping.constEnd(); ++it) {
            if (attrIndexByName(it.value()) >= 0)
                ++ratedCount;
        }
        QCOMPARE(ratedCount, kAttrCount);
    }
};

QTEST_APPLESS_MAIN(TestPlayerStore)
#include "test_playerstore.moc"
