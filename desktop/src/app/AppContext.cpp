#include "AppContext.h"

#include <QDir>

namespace fm {

AppContext::AppContext(QObject *parent)
    : QObject(parent)
{
}

AppContext::~AppContext() = default;

bool AppContext::initialize(QString *errorOut)
{
    if (!m_paths.ensureDataDirInitialized()) {
        if (errorOut)
            *errorOut = tr("Datenordner %1 konnte nicht angelegt werden.").arg(m_paths.dataDir());
        return false;
    }

    m_config = std::make_unique<AppConfig>(m_paths.configFile());

    m_definitions = std::make_unique<Definitions>();
    if (!m_definitions->load(m_paths.definitionsFile())) {
        if (errorOut)
            *errorOut = m_definitions->errorString();
        return false;
    }

    m_dwrsEngine = std::make_unique<DwrsEngine>(*m_definitions, *m_config);
    m_squadBuilder = std::make_unique<SquadBuilder>(*m_definitions, *m_config);
    m_tacticExplorer = std::make_unique<TacticExplorer>(*m_definitions, *m_squadBuilder);

    return openDatabase(m_config->dbName(), errorOut);
}

QString AppContext::currentDbName() const
{
    return m_config->dbName();
}

QStringList AppContext::availableDatabases() const
{
    QDir dir(m_paths.databasesDir());
    QStringList names;
    const QStringList files = dir.entryList({QStringLiteral("*.db")}, QDir::Files, QDir::Name);
    for (const QString &file : files)
        names.append(file.chopped(3)); // strip ".db"
    return names;
}

bool AppContext::openDatabase(const QString &dbName, QString *errorOut)
{
    auto database = std::make_unique<Database>(QStringLiteral("main_%1").arg(dbName));
    if (!database->open(m_paths.databaseFile(dbName))) {
        if (errorOut)
            *errorOut = database->errorString();
        return false;
    }
    m_database = std::move(database);
    m_config->setDbName(dbName);

    reloadFromDatabase();
    emit databaseChanged(dbName);
    return true;
}

void AppContext::reloadFromDatabase()
{
    m_store.reset(m_database->loadPlayers());

    m_latestRatings = m_database->latestDwrsRatings();
    m_ratings.clear();
    for (auto it = m_latestRatings.constBegin(); it != m_latestRatings.constEnd(); ++it) {
        const int row = m_store.rowById(it.key().first);
        if (row >= 0)
            m_ratings[it.key().second].insert(m_store.at(row).uid, it.value().second);
    }
    emit dataChanged();
}

void AppContext::reloadEngines()
{
    m_dwrsEngine->reloadConfig();
    m_squadBuilder->reloadConfig();
}

void AppContext::reloadConfigAndDefinitions()
{
    m_config->reload();
    m_definitions->load(m_paths.definitionsFile());
    reloadEngines();
    emit dataChanged();
}

} // namespace fm
