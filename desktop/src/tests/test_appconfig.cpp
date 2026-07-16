#include <QtTest>

#include "core/AppConfig.h"

using namespace fm;

class TestAppConfig : public QObject
{
    Q_OBJECT

private slots:
    void seedsDefaultsOnFirstRun()
    {
        QTemporaryDir dir;
        AppConfig config(dir.filePath(QStringLiteral("config.ini")));

        QCOMPARE(config.dbName(), QStringLiteral("default"));
        QCOMPARE(config.weight(QStringLiteral("Extremely Important")), 8.0);
        QCOMPARE(config.weight(QStringLiteral("Almost Irrelevant")), 0.2);
        QCOMPARE(config.gkWeight(QStringLiteral("Top Importance")), 10.0);
        QCOMPARE(config.roleMultiplier(QStringLiteral("key")), 1.5);
        QCOMPARE(config.roleMultiplier(QStringLiteral("preferable")), 1.2);
        QCOMPARE(config.aptWeight(QStringLiteral("Star Player")), 1.0);
        QCOMPARE(config.ageThreshold(QStringLiteral("outfielder")), 20);
        QCOMPARE(config.ageThreshold(QStringLiteral("goalkeeper")), 25);
        QCOMPARE(config.selectionBonus(QStringLiteral("natural_position")), 1.0);
        QCOMPARE(config.squadManagementSetting(QStringLiteral("max_roles_per_depth_player")), 2);
        QCOMPARE(config.squadManagementSetting(QStringLiteral("min_loan_talent_score")), 45);
        QCOMPARE(config.gapAnalysisSetting(QStringLiteral("displacement_threshold")), 8.0);
        QCOMPARE(config.themeSettings().value(QStringLiteral("current_mode")),
                 QStringLiteral("night"));

        // The file must exist on disk after seeding.
        QVERIFY(QFile::exists(config.filePath()));
    }

    void aptWeightNoneReturnsDefault()
    {
        QTemporaryDir dir;
        AppConfig config(dir.filePath(QStringLiteral("config.ini")));
        QCOMPARE(config.aptWeight(QStringLiteral("None"), 1.0), 1.0);
        QCOMPARE(config.aptWeight(QString(), 0.7), 0.7);
    }

    void setAndReload()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.ini"));
        {
            AppConfig config(path);
            config.setDbName(QStringLiteral("bayern2026"));
            config.setWeight(QStringLiteral("Extremely Important"), 10.0);
            config.setRoleMultiplier(QStringLiteral("key"), 8.0);
            config.setAptWeight(QStringLiteral("Star Player"), 1.3);
            config.setGapAnalysisSetting(QStringLiteral("dropoff_threshold"), 6.5);
        }
        AppConfig reloaded(path);
        QCOMPARE(reloaded.dbName(), QStringLiteral("bayern2026"));
        QCOMPARE(reloaded.weight(QStringLiteral("Extremely Important")), 10.0);
        QCOMPARE(reloaded.roleMultiplier(QStringLiteral("key")), 8.0);
        QCOMPARE(reloaded.aptWeight(QStringLiteral("Star Player")), 1.3);
        QCOMPARE(reloaded.gapAnalysisSetting(QStringLiteral("dropoff_threshold")), 6.5);
    }

    void htmlExportDirFallsBackToDefault()
    {
        QTemporaryDir dir;
        AppConfig config(dir.filePath(QStringLiteral("config.ini")));

        // Unset -> automatic default (Documents-based, never empty).
        QCOMPARE(config.htmlExportDir(), QString());
        QCOMPARE(config.effectiveHtmlExportDir(), AppConfig::defaultHtmlExportDir());
        QVERIFY(!config.effectiveHtmlExportDir().isEmpty());

        // Existing configured folder wins.
        config.setHtmlExportDir(dir.path());
        QCOMPARE(config.effectiveHtmlExportDir(), dir.path());

        // A configured folder that does not exist falls back to the default.
        config.setHtmlExportDir(dir.filePath(QStringLiteral("does-not-exist")));
        QCOMPARE(config.effectiveHtmlExportDir(), AppConfig::defaultHtmlExportDir());

        // Clearing removes the key again.
        config.setHtmlExportDir(QString());
        QCOMPARE(config.htmlExportDir(), QString());
    }

    void readsLegacyStyleIni()
    {
        // A user's tuned legacy config.ini must be readable unchanged.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.ini"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[Database]\n"
                    "db_name = bayern2026-2-0\n"
                    "\n"
                    "[Weights]\n"
                    "extremely_important = 10.0\n"
                    "important = 4.0\n"
                    "\n"
                    "[RoleMultipliers]\n"
                    "key_multiplier = 8.0\n");
        }
        AppConfig config(path);
        QCOMPARE(config.dbName(), QStringLiteral("bayern2026-2-0"));
        QCOMPARE(config.weight(QStringLiteral("Extremely Important")), 10.0);
        QCOMPARE(config.roleMultiplier(QStringLiteral("key")), 8.0);
        // Sections missing from the file get seeded with defaults.
        QCOMPARE(config.ageThreshold(QStringLiteral("goalkeeper")), 25);
    }
};

QTEST_APPLESS_MAIN(TestAppConfig)
#include "test_appconfig.moc"
