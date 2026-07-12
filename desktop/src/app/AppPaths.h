#pragma once

#include <QSettings>
#include <QString>

#include <memory>

namespace fm {

// Bootstrap settings in %APPDATA%\FM24PlayerAnalyzer\bootstrap.ini: only the
// data directory location, window geometry and similar host-level state.
// Everything else (weights, theme colors, active DB) lives in
// <dataDir>/config.ini so the whole data directory stays self-contained and
// portable.
class AppPaths
{
public:
    AppPaths();

    // True until a data directory has been chosen (first run).
    bool isFirstRun() const;

    // The data directory (default: %LOCALAPPDATA%\FM24PlayerAnalyzer).
    QString dataDir() const;
    void setDataDir(const QString &dir);
    static QString defaultDataDir();

    // Derived locations inside the data directory.
    QString databasesDir() const;
    QString backupsDir() const;
    QString assetsDir() const;
    QString configFile() const;      // <dataDir>/config.ini
    QString definitionsFile() const; // <dataDir>/definitions.json
    QString databaseFile(const QString &dbName) const;

    // Creates the directory skeleton and seeds definitions.json from the
    // bundled default if missing. Returns false on IO failure.
    bool ensureDataDirInitialized();

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    // UI language, app-global (host-level). "en" (default) loads the bundled
    // English translation; "de" uses the untranslated German source strings.
    QString language() const;
    void setLanguage(const QString &language);

private:
    std::unique_ptr<QSettings> m_settings;
};

} // namespace fm
