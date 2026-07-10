#pragma once

#include "AppPaths.h"
#include "core/AppConfig.h"
#include "core/Database.h"
#include "core/Definitions.h"
#include "core/DwrsEngine.h"
#include "core/PlayerStore.h"
#include "core/SquadBuilder.h"
#include "core/TacticExplorer.h"

#include <QObject>

#include <memory>

namespace fm {

// Owns all application state: paths, config, definitions, the open database,
// the in-memory player store, the analysis engines and the ratings caches.
// Pages read from here and connect to dataChanged().
class AppContext : public QObject
{
    Q_OBJECT

public:
    explicit AppContext(QObject *parent = nullptr);
    ~AppContext() override;

    // Call after the data dir is settled (first-run dialog done). Loads
    // config/definitions and opens the active database.
    bool initialize(QString *errorOut = nullptr);

    AppPaths &paths() { return m_paths; }
    AppConfig &config() { return *m_config; }
    Definitions &definitions() { return *m_definitions; }
    Database &database() { return *m_database; }
    PlayerStore &store() { return m_store; }
    DwrsEngine &dwrsEngine() { return *m_dwrsEngine; }
    SquadBuilder &squadBuilder() { return *m_squadBuilder; }
    TacticExplorer &tacticExplorer() { return *m_tacticExplorer; }

    // uid-keyed latest normalized ratings (master_role_ratings equivalent).
    const RoleRatings &ratings() const { return m_ratings; }
    // id-keyed latest (absolute, normalized).
    const LatestRatings &latestRatings() const { return m_latestRatings; }

    QString currentDbName() const;
    QStringList availableDatabases() const;

    // Switches/creates the active database and reloads everything.
    bool openDatabase(const QString &dbName, QString *errorOut = nullptr);

    // Re-reads players + ratings from the database into memory.
    void reloadFromDatabase();

    // Re-reads config-dependent engine state (after settings changes).
    void reloadEngines();

    // Re-reads config.ini and definitions.json from disk (after the migration
    // wizard replaced them) and refreshes the engines.
    void reloadConfigAndDefinitions();

    // Convenience settings accessors (stored in the DB settings table).
    QString userClub() { return m_database->setting(QStringLiteral("user_club")); }
    QString secondTeamClub() { return m_database->setting(QStringLiteral("second_team_club")); }
    bool nationalModeEnabled()
    {
        return m_database->setting(QStringLiteral("national_mode_enabled")) == QLatin1String("true");
    }

signals:
    // Player data or ratings changed — pages must refresh.
    void dataChanged();
    // A different database file was opened.
    void databaseChanged(const QString &dbName);

private:
    AppPaths m_paths;
    std::unique_ptr<AppConfig> m_config;
    std::unique_ptr<Definitions> m_definitions;
    std::unique_ptr<Database> m_database;
    PlayerStore m_store;
    std::unique_ptr<DwrsEngine> m_dwrsEngine;
    std::unique_ptr<SquadBuilder> m_squadBuilder;
    std::unique_ptr<TacticExplorer> m_tacticExplorer;
    RoleRatings m_ratings;
    LatestRatings m_latestRatings;
};

} // namespace fm
