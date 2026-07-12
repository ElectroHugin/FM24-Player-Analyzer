#include "AppConfig.h"

#include "Constants.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace fm {

namespace {

const QHash<QString, double> &gapAnalysisDefaults()
{
    static const QHash<QString, double> defaults = {
        {QStringLiteral("displacement_threshold"), 8.0},
        {QStringLiteral("dropoff_threshold"), 8.0},
        {QStringLiteral("wrong_side_penalty"), 5.0},
    };
    return defaults;
}

const QHash<QString, int> &squadManagementDefaults()
{
    static const QHash<QString, int> defaults = {
        {QStringLiteral("max_roles_per_depth_player"), 2},
        {QStringLiteral("min_loan_talent_score"), 45},
    };
    return defaults;
}

const QHash<QString, QString> &themeDefaults()
{
    // Club-neutral, professional dark/light base (seven color roles per mode).
    // Club presets override primary/interactive/header on top of this.
    static const QHash<QString, QString> defaults = {
        {QStringLiteral("current_mode"), QStringLiteral("night")},
        {QStringLiteral("theme_preset"), QStringLiteral("neutral")},
        // --- Night ---
        {QStringLiteral("night_background_color"), QStringLiteral("#262626")},
        {QStringLiteral("night_surface_color"), QStringLiteral("#343434")},
        {QStringLiteral("night_sidebar_color"), QStringLiteral("#1f1f1f")},
        {QStringLiteral("night_header_color"), QStringLiteral("#2d2d2d")},
        {QStringLiteral("night_primary_color"), QStringLiteral("#3d7dff")},
        {QStringLiteral("night_interactive_color"), QStringLiteral("#3d7dff")},
        {QStringLiteral("night_text_color"), QStringLiteral("#F5F5F5")},
        // --- Day ---
        {QStringLiteral("day_background_color"), QStringLiteral("#F0F2F6")},
        {QStringLiteral("day_surface_color"), QStringLiteral("#FFFFFF")},
        {QStringLiteral("day_sidebar_color"), QStringLiteral("#E7E9ED")},
        {QStringLiteral("day_header_color"), QStringLiteral("#FFFFFF")},
        {QStringLiteral("day_primary_color"), QStringLiteral("#0069b3")},
        {QStringLiteral("day_interactive_color"), QStringLiteral("#0069b3")},
        {QStringLiteral("day_text_color"), QStringLiteral("#31333F")},
    };
    return defaults;
}

} // namespace

AppConfig::AppConfig(const QString &configFilePath)
    : m_filePath(configFilePath)
{
    QDir().mkpath(QFileInfo(configFilePath).absolutePath());
    m_settings = std::make_unique<QSettings>(configFilePath, QSettings::IniFormat);
    ensureDefaults();
}

void AppConfig::reload()
{
    m_settings = std::make_unique<QSettings>(m_filePath, QSettings::IniFormat);
    ensureDefaults();
}

QString AppConfig::iniKey(const QString &name)
{
    return name.toLower().replace(QLatin1Char(' '), QLatin1Char('_'));
}

void AppConfig::ensureDefaults()
{
    QSettings &s = *m_settings;
    bool modified = false;

    const auto ensure = [&](const QString &fullKey, const QVariant &value) {
        if (!s.contains(fullKey)) {
            s.setValue(fullKey, value);
            modified = true;
        }
    };

    ensure(QStringLiteral("Database/db_name"), QStringLiteral("default"));

    const auto &weights = weightDefaults();
    for (auto it = weights.constBegin(); it != weights.constEnd(); ++it)
        ensure(QStringLiteral("Weights/") + iniKey(it.key()), QString::number(it.value()));

    const auto &gkWeights = gkWeightDefaults();
    for (auto it = gkWeights.constBegin(); it != gkWeights.constEnd(); ++it)
        ensure(QStringLiteral("GKWeights/") + iniKey(it.key()), QString::number(it.value()));

    ensure(QStringLiteral("RoleMultipliers/key_multiplier"), QStringLiteral("1.5"));
    ensure(QStringLiteral("RoleMultipliers/preferable_multiplier"), QStringLiteral("1.2"));

    QSet<QString> allApt;
    for (const QString &apt : fieldPlayerAptOptions())
        allApt.insert(apt);
    for (const QString &apt : gkAptOptions())
        allApt.insert(apt);
    for (const QString &apt : allApt) {
        if (apt != QLatin1String("None"))
            ensure(QStringLiteral("APTWeights/") + iniKey(apt), QStringLiteral("1.0"));
    }

    ensure(QStringLiteral("AgeThresholds/outfielder_youth_age"), QStringLiteral("20"));
    ensure(QStringLiteral("AgeThresholds/goalkeeper_youth_age"), QStringLiteral("25"));

    ensure(QStringLiteral("SelectionBonuses/natural_position_multiplier"), QStringLiteral("1.00"));

    const auto &squad = squadManagementDefaults();
    for (auto it = squad.constBegin(); it != squad.constEnd(); ++it)
        ensure(QStringLiteral("SquadManagement/") + it.key(), QString::number(it.value()));

    const auto &gap = gapAnalysisDefaults();
    for (auto it = gap.constBegin(); it != gap.constEnd(); ++it)
        ensure(QStringLiteral("GapAnalysis/") + it.key(), QString::number(it.value()));

    const auto &theme = themeDefaults();
    for (auto it = theme.constBegin(); it != theme.constEnd(); ++it)
        ensure(QStringLiteral("ThemeSettings/") + it.key(), it.value());

    if (modified)
        s.sync();
}

QString AppConfig::dbName() const
{
    return m_settings->value(QStringLiteral("Database/db_name"), QStringLiteral("default"))
        .toString();
}

void AppConfig::setDbName(const QString &name)
{
    m_settings->setValue(QStringLiteral("Database/db_name"), name);
    m_settings->sync();
}

double AppConfig::weight(const QString &category) const
{
    const double fallback = weightDefaults().value(category, 1.0);
    return m_settings->value(QStringLiteral("Weights/") + iniKey(category), fallback).toDouble();
}

void AppConfig::setWeight(const QString &category, double value)
{
    m_settings->setValue(QStringLiteral("Weights/") + iniKey(category), QString::number(value));
    m_settings->sync();
}

double AppConfig::gkWeight(const QString &category) const
{
    const double fallback = gkWeightDefaults().value(category, 1.0);
    return m_settings->value(QStringLiteral("GKWeights/") + iniKey(category), fallback).toDouble();
}

void AppConfig::setGkWeight(const QString &category, double value)
{
    m_settings->setValue(QStringLiteral("GKWeights/") + iniKey(category), QString::number(value));
    m_settings->sync();
}

double AppConfig::roleMultiplier(const QString &type) const
{
    const double fallback = type == QLatin1String("key") ? kKeyMultiplierDefault
                                                         : kPreferableMultiplierDefault;
    return m_settings
        ->value(QStringLiteral("RoleMultipliers/") + type + QStringLiteral("_multiplier"), fallback)
        .toDouble();
}

void AppConfig::setRoleMultiplier(const QString &type, double value)
{
    m_settings->setValue(QStringLiteral("RoleMultipliers/") + type + QStringLiteral("_multiplier"),
                         QString::number(value));
    m_settings->sync();
}

double AppConfig::aptWeight(const QString &apt, double defaultValue) const
{
    if (apt.isEmpty() || apt == QLatin1String("None"))
        return defaultValue;
    return m_settings->value(QStringLiteral("APTWeights/") + iniKey(apt), defaultValue).toDouble();
}

void AppConfig::setAptWeight(const QString &apt, double value)
{
    if (apt.isEmpty() || apt == QLatin1String("None"))
        return;
    m_settings->setValue(QStringLiteral("APTWeights/") + iniKey(apt), QString::number(value));
    m_settings->sync();
}

int AppConfig::ageThreshold(const QString &playerType) const
{
    const int fallback = playerType == QLatin1String("goalkeeper") ? 25 : 20;
    return m_settings
        ->value(QStringLiteral("AgeThresholds/") + playerType + QStringLiteral("_youth_age"),
                fallback)
        .toInt();
}

void AppConfig::setAgeThreshold(const QString &playerType, int value)
{
    m_settings->setValue(QStringLiteral("AgeThresholds/") + playerType
                             + QStringLiteral("_youth_age"),
                         QString::number(value));
    m_settings->sync();
}

double AppConfig::selectionBonus(const QString &key) const
{
    // Note: legacy get_selection_bonus falls back to 1.05 for
    // "natural_position" but load_config seeds the file with 1.00, so the
    // seeded value is what takes effect in practice.
    const double fallback = key == QLatin1String("natural_position") ? 1.05 : 1.0;
    return m_settings
        ->value(QStringLiteral("SelectionBonuses/") + key + QStringLiteral("_multiplier"), fallback)
        .toDouble();
}

void AppConfig::setSelectionBonus(const QString &key, double value)
{
    m_settings->setValue(QStringLiteral("SelectionBonuses/") + key + QStringLiteral("_multiplier"),
                         QString::number(value));
    m_settings->sync();
}

int AppConfig::squadManagementSetting(const QString &key) const
{
    const int fallback = squadManagementDefaults().value(key, 0);
    return m_settings->value(QStringLiteral("SquadManagement/") + key, fallback).toInt();
}

void AppConfig::setSquadManagementSetting(const QString &key, int value)
{
    m_settings->setValue(QStringLiteral("SquadManagement/") + key, QString::number(value));
    m_settings->sync();
}

double AppConfig::gapAnalysisSetting(const QString &key) const
{
    const double fallback = gapAnalysisDefaults().value(key, 0.0);
    return m_settings->value(QStringLiteral("GapAnalysis/") + key, fallback).toDouble();
}

void AppConfig::setGapAnalysisSetting(const QString &key, double value)
{
    m_settings->setValue(QStringLiteral("GapAnalysis/") + key, QString::number(value));
    m_settings->sync();
}

QHash<QString, QString> AppConfig::themeSettings() const
{
    QHash<QString, QString> result = themeDefaults();
    m_settings->beginGroup(QStringLiteral("ThemeSettings"));
    const QStringList keys = m_settings->childKeys();
    for (const QString &key : keys)
        result.insert(key, m_settings->value(key).toString());
    m_settings->endGroup();
    return result;
}

void AppConfig::saveThemeSettings(const QHash<QString, QString> &settings)
{
    m_settings->beginGroup(QStringLiteral("ThemeSettings"));
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it)
        m_settings->setValue(it.key(), it.value());
    m_settings->endGroup();
    m_settings->sync();
}

} // namespace fm
