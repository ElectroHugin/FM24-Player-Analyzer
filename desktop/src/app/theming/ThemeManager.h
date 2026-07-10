#pragma once

#include <QColor>
#include <QObject>
#include <QString>

namespace fm {

class AppConfig;

// Applies the day/night theme from [ThemeSettings] in config.ini as an
// app-wide stylesheet and exposes the palette to custom-painted widgets.
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(AppConfig &config, QObject *parent = nullptr);

    QString mode() const { return m_mode; } // "day" | "night"
    void setMode(const QString &mode);
    void toggle();

    // Re-read colors from config (after the settings page saved them).
    void apply();

    QColor primary() const { return m_primary; }
    QColor text() const { return m_text; }
    QColor background() const { return m_background; }
    QColor secondaryBackground() const { return m_secondaryBackground; }

signals:
    void themeChanged();

private:
    QString buildStyleSheet() const;

    AppConfig &m_config;
    QString m_mode;
    QColor m_primary, m_text, m_background, m_secondaryBackground;
};

} // namespace fm
