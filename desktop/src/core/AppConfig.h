#pragma once

#include <QHash>
#include <QSettings>
#include <QString>

#include <memory>

namespace fm {

// App settings stored in <dataDir>/config.ini, mirroring the legacy
// config_handler.py sections and keys 1:1 so a user's tuned legacy config.ini
// can be copied in unchanged. Missing sections are created with the legacy
// defaults on construction.
class AppConfig
{
public:
    explicit AppConfig(const QString &configFilePath);

    QString filePath() const { return m_filePath; }

    // Re-reads the file from disk (e.g. after the migration wizard replaced
    // it) and re-seeds any missing sections.
    void reload();

    // [Database]
    QString dbName() const;
    void setDbName(const QString &name);

    // [Weights] / [GKWeights] — category name as in constants ("Extremely
    // Important", "Top Importance", ...).
    double weight(const QString &category) const;
    void setWeight(const QString &category, double value);
    double gkWeight(const QString &category) const;
    void setGkWeight(const QString &category, double value);

    // [RoleMultipliers] — type "key" | "preferable".
    double roleMultiplier(const QString &type) const;
    void setRoleMultiplier(const QString &type, double value);

    // [APTWeights] — APT display name ("Star Player", ...); "None"/empty -> default.
    double aptWeight(const QString &apt, double defaultValue = 1.0) const;
    void setAptWeight(const QString &apt, double value);

    // [AgeThresholds] — playerType "outfielder" | "goalkeeper".
    int ageThreshold(const QString &playerType) const;
    void setAgeThreshold(const QString &playerType, int value);

    // [SelectionBonuses] — key without "_multiplier" suffix, e.g. "natural_position".
    double selectionBonus(const QString &key) const;
    void setSelectionBonus(const QString &key, double value);

    // [SquadManagement]
    int squadManagementSetting(const QString &key) const;
    void setSquadManagementSetting(const QString &key, int value);

    // [GapAnalysis]
    double gapAnalysisSetting(const QString &key) const;
    void setGapAnalysisSetting(const QString &key, double value);

    // [ThemeSettings] — full key/value map ("current_mode", "night_primary_color", ...).
    QHash<QString, QString> themeSettings() const;
    void saveThemeSettings(const QHash<QString, QString> &settings);

    // [Import] — user-configured default folder for the FM-HTML-export file
    // dialogs. Empty = automatic default (see defaultHtmlExportDir).
    QString htmlExportDir() const;
    void setHtmlExportDir(const QString &dir);
    // "<Documents>/Sports Interactive/Football Manager 2024" of the current
    // user when that folder exists, otherwise the Documents folder itself.
    static QString defaultHtmlExportDir();
    // Configured folder if set and existing, otherwise defaultHtmlExportDir().
    QString effectiveHtmlExportDir() const;

private:
    void ensureDefaults();
    static QString iniKey(const QString &name); // "Star Player" -> "star_player"

    QString m_filePath;
    std::unique_ptr<QSettings> m_settings;
};

} // namespace fm
