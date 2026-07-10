#include <QtTest>

#include "core/Utils.h"

using namespace fm;

class TestUtils : public QObject
{
    Q_OBJECT

private slots:
    void valueToFloat_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<double>("expected");

        QTest::newRow("millions") << QStringLiteral("€1.2M") << 1'200'000.0;
        QTest::newRow("thousands") << QStringLiteral("€500K") << 500'000.0;
        QTest::newRow("plain") << QStringLiteral("€750") << 750.0;
        QTest::newRow("range takes lower") << QStringLiteral("€500K - €800K") << 500'000.0;
        QTest::newRow("not for sale") << QStringLiteral("Not for Sale") << 2'000'000'000.0;
        QTest::newRow("not for sale lowercase") << QStringLiteral("not for sale") << 2'000'000'000.0;
        QTest::newRow("empty") << QString() << 0.0;
        QTest::newRow("garbage") << QStringLiteral("Unknown") << 0.0;
        QTest::newRow("no euro sign") << QStringLiteral("2.5M") << 2'500'000.0;
    }

    void valueToFloat()
    {
        QFETCH(QString, input);
        QFETCH(double, expected);
        QCOMPARE(fm::valueToFloat(input), expected);
    }

    void parsePositionString_complex()
    {
        const auto result = fm::parsePositionString(QStringLiteral("AM (RL), ST (C)"));
        const QSet<QString> expected = {QStringLiteral("AM (R)"), QStringLiteral("AM (L)"),
                                        QStringLiteral("ST (C)")};
        QCOMPARE(result, expected);
    }

    void parsePositionString_slashBases()
    {
        const auto result = fm::parsePositionString(QStringLiteral("D/WB (R)"));
        const QSet<QString> expected = {QStringLiteral("D (R)"), QStringLiteral("WB (R)")};
        QCOMPARE(result, expected);
    }

    void parsePositionString_bareStBecomesCentral()
    {
        const auto result = fm::parsePositionString(QStringLiteral("ST"));
        QCOMPARE(result, QSet<QString>{QStringLiteral("ST (C)")});
    }

    void parsePositionString_sidelessStaysBare()
    {
        const auto result = fm::parsePositionString(QStringLiteral("DM, M (C)"));
        const QSet<QString> expected = {QStringLiteral("DM"), QStringLiteral("M (C)")};
        QCOMPARE(result, expected);
    }

    void parsePositionString_empty()
    {
        QVERIFY(fm::parsePositionString(QString()).isEmpty());
    }

    void getLastName()
    {
        QCOMPARE(fm::getLastName(QStringLiteral("Erling Braut Haaland")),
                 QStringLiteral("Haaland"));
        QCOMPARE(fm::getLastName(QStringLiteral("Pelé")), QStringLiteral("Pelé"));
        QCOMPARE(fm::getLastName(QString()), QString());
    }

    void contrastRatio()
    {
        // Black on white is the WCAG maximum, 21:1.
        QCOMPARE(fm::contrastRatio(QColorConstants::Black, QColorConstants::White), 21.0);
        // Identical colors have ratio 1.
        QCOMPARE(fm::contrastRatio(QColorConstants::White, QColorConstants::White), 1.0);
    }

    void attributeCellStyle_tiers()
    {
        QCOMPARE(fm::attributeCellStyle(20).background.name(), QStringLiteral("#0da025"));
        QCOMPARE(fm::attributeCellStyle(18).background.name(), QStringLiteral("#0da025"));
        QCOMPARE(fm::attributeCellStyle(17).background.name(), QStringLiteral("#a2d31a"));
        QCOMPARE(fm::attributeCellStyle(14).background.name(), QStringLiteral("#d8d21e"));
        QCOMPARE(fm::attributeCellStyle(11).background.name(), QStringLiteral("#ca7b3a"));
        QCOMPARE(fm::attributeCellStyle(7).background.name(), QStringLiteral("#cf1e1e"));
        QVERIFY(!fm::attributeCellStyle(0).isValid());
    }

    void dwrsCellStyle_endpoints()
    {
        // vmin -> first LUT entry (red end of gist_rainbow: 255, 0, 41).
        const auto low = fm::dwrsCellStyle(30.0);
        QCOMPARE(low.background.red(), 255);
        QCOMPARE(low.background.green(), 0);
        // vmax -> last LUT entry (magenta end: 255, 0, 191).
        const auto high = fm::dwrsCellStyle(95.0);
        QCOMPARE(high.background.red(), 255);
        QCOMPARE(high.background.blue(), 191);
        // Out-of-range values clamp.
        QCOMPARE(fm::dwrsCellStyle(120.0).background, high.background);
        QCOMPARE(fm::dwrsCellStyle(0.0).background, low.background);
    }
};

QTEST_APPLESS_MAIN(TestUtils)
#include "test_utils.moc"
