#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fm {

class AppConfig;

// Applies the day/night theme from [ThemeSettings] in config.ini as an
// app-wide stylesheet and exposes the palette to custom-painted widgets.
//
// The theme is described by seven named color roles per mode so color can be
// distributed instead of one "secondary background" tinting everything:
//   background   – page background
//   surface      – panels, cards, inputs, group boxes
//   sidebar      – navigation sidebar background
//   header       – top identity bar background
//   primary      – primary buttons, active nav entry (the club's main color)
//   interactive  – tabs, sliders, checkboxes, focus, selection (the accent)
//   text         – primary text
// border and muted text are derived from these.
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(AppConfig &config, QObject *parent = nullptr);

    // The seven configurable role names (without the "night_"/"day_" prefix
    // and "_color" suffix used as settings keys).
    static const QStringList &roleNames();
    // Settings key for a role in a mode, e.g. ("night","primary") ->
    // "night_primary_color".
    static QString settingsKey(const QString &mode, const QString &role);

    QString mode() const { return m_mode; } // "day" | "night"
    void setMode(const QString &mode);
    void toggle();

    // Re-read colors from config (after the settings page saved them).
    void apply();

    // --- Role colors (current mode) ---
    QColor background() const { return m_background; }
    QColor surface() const { return m_surface; }
    QColor sidebar() const { return m_sidebar; }
    QColor header() const { return m_header; }
    QColor primary() const { return m_primary; }
    QColor interactive() const { return m_interactive; }
    QColor text() const { return m_text; }
    // Backward-compatible alias used by custom-painted widgets.
    QColor secondaryBackground() const { return m_surface; }

signals:
    void themeChanged();

private:
    QString buildStyleSheet() const;
    QColor roleColor(const QString &role) const;

    AppConfig &m_config;
    QString m_mode;
    QColor m_background, m_surface, m_sidebar, m_header, m_primary, m_interactive, m_text;
};

} // namespace fm
