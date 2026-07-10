#include <QtTest>

#include "core/Version.h"

class TestSmoke : public QObject
{
    Q_OBJECT

private slots:
    void version()
    {
        QVERIFY(!fm::appVersion().isEmpty());
        QCOMPARE(fm::appName(), QStringLiteral("FM24 Player Analyzer"));
    }
};

QTEST_APPLESS_MAIN(TestSmoke)
#include "test_smoke.moc"
