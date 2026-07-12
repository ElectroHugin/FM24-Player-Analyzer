#pragma once

#include <QHash>
#include <QList>
#include <QString>

namespace fm {

// A color preset: a club (or the neutral default). Only the brand colors are
// stored; the neutral anthracite/light base is shared by all presets so club
// color stays an accent (primary/interactive/header) on a professional grey
// foundation. header* empty means "use primary as the header color".
struct ClubTheme {
    QString id;
    QString displayName;
    QString primary;      // main brand color (buttons, active nav, header)
    QString interactive;  // accent for tabs/sliders/checkboxes/selection
    QString headerNight;  // optional header override (dark mode)
    QString headerDay;    // optional header override (light mode)
};

// All presets, "neutral" first, then Bundesliga, then international clubs.
const QList<ClubTheme> &clubThemes();

// Full theme settings (every role, both modes, plus theme_preset) for a
// preset id; falls back to "neutral".
QHash<QString, QString> presetThemeSettings(const QString &presetId);

} // namespace fm
