#include "ThemePresets.h"

namespace fm {

const QList<ClubTheme> &clubThemes()
{
    // header override only where the club's identity is a dark second color
    // (e.g. black/yellow), so the header isn't a glaring full-brand bar.
    static const QString blk = QStringLiteral("#161616"); // near-black header
    static const QList<ClubTheme> themes = {
        {QStringLiteral("neutral"), QStringLiteral("Vereinsneutral (Standard)"),
         QStringLiteral("#3d7dff"), QStringLiteral("#3d7dff"), QStringLiteral("#2d2d2d"),
         QStringLiteral("#ffffff")},

        // --- Bundesliga ---
        {QStringLiteral("fcbayern"), QStringLiteral("FC Bayern München"),
         QStringLiteral("#dc052d"), QStringLiteral("#0066b2"), {}, {}},
        {QStringLiteral("bvb"), QStringLiteral("Borussia Dortmund"),
         QStringLiteral("#fde100"), QStringLiteral("#fde100"), blk, blk},
        {QStringLiteral("rbleipzig"), QStringLiteral("RB Leipzig"),
         QStringLiteral("#dd0741"), QStringLiteral("#0a3d8f"), {}, {}},
        {QStringLiteral("leverkusen"), QStringLiteral("Bayer 04 Leverkusen"),
         QStringLiteral("#e32219"), QStringLiteral("#e32219"), blk, blk},
        {QStringLiteral("stuttgart"), QStringLiteral("VfB Stuttgart"),
         QStringLiteral("#e32219"), QStringLiteral("#e32219"), {}, {}},
        {QStringLiteral("frankfurt"), QStringLiteral("Eintracht Frankfurt"),
         QStringLiteral("#e1000f"), QStringLiteral("#e1000f"), blk, blk},
        {QStringLiteral("hoffenheim"), QStringLiteral("TSG Hoffenheim"),
         QStringLiteral("#1c63b7"), QStringLiteral("#1c63b7"), {}, {}},
        {QStringLiteral("freiburg"), QStringLiteral("SC Freiburg"),
         QStringLiteral("#e2001a"), QStringLiteral("#e2001a"), blk, blk},
        {QStringLiteral("bremen"), QStringLiteral("SV Werder Bremen"),
         QStringLiteral("#1d9053"), QStringLiteral("#1d9053"), {}, {}},
        {QStringLiteral("augsburg"), QStringLiteral("FC Augsburg"),
         QStringLiteral("#ba3733"), QStringLiteral("#46714d"), {}, {}},
        {QStringLiteral("wolfsburg"), QStringLiteral("VfL Wolfsburg"),
         QStringLiteral("#65b32e"), QStringLiteral("#65b32e"), {}, {}},
        {QStringLiteral("mainz"), QStringLiteral("1. FSV Mainz 05"),
         QStringLiteral("#c3141e"), QStringLiteral("#c3141e"), {}, {}},
        {QStringLiteral("gladbach"), QStringLiteral("Borussia Mönchengladbach"),
         QStringLiteral("#00a651"), QStringLiteral("#00a651"), blk, blk},
        {QStringLiteral("union"), QStringLiteral("1. FC Union Berlin"),
         QStringLiteral("#eb1923"), QStringLiteral("#ffd200"), {}, {}},
        {QStringLiteral("bochum"), QStringLiteral("VfL Bochum"),
         QStringLiteral("#005ca9"), QStringLiteral("#005ca9"), {}, {}},
        {QStringLiteral("heidenheim"), QStringLiteral("1. FC Heidenheim"),
         QStringLiteral("#e30613"), QStringLiteral("#003d7c"), {}, {}},
        {QStringLiteral("stpauli"), QStringLiteral("FC St. Pauli"),
         QStringLiteral("#6b4423"), QStringLiteral("#e30613"), {}, {}},
        {QStringLiteral("kiel"), QStringLiteral("Holstein Kiel"),
         QStringLiteral("#005aa0"), QStringLiteral("#e30613"), {}, {}},

        // --- International ---
        {QStringLiteral("realmadrid"), QStringLiteral("Real Madrid"),
         QStringLiteral("#00529f"), QStringLiteral("#febe10"), {}, {}},
        {QStringLiteral("barcelona"), QStringLiteral("FC Barcelona"),
         QStringLiteral("#a50044"), QStringLiteral("#004d98"), {}, {}},
        {QStringLiteral("atletico"), QStringLiteral("Atlético Madrid"),
         QStringLiteral("#cb3524"), QStringLiteral("#262e62"), {}, {}},
        {QStringLiteral("mancity"), QStringLiteral("Manchester City"),
         QStringLiteral("#6cabdd"), QStringLiteral("#1c2c5b"), {}, {}},
        {QStringLiteral("manutd"), QStringLiteral("Manchester United"),
         QStringLiteral("#da020e"), QStringLiteral("#fbe122"), {}, {}},
        {QStringLiteral("liverpool"), QStringLiteral("Liverpool FC"),
         QStringLiteral("#c8102e"), QStringLiteral("#00b2a9"), {}, {}},
        {QStringLiteral("arsenal"), QStringLiteral("Arsenal FC"),
         QStringLiteral("#ef0107"), QStringLiteral("#063672"), {}, {}},
        {QStringLiteral("chelsea"), QStringLiteral("Chelsea FC"),
         QStringLiteral("#034694"), QStringLiteral("#034694"), {}, {}},
        {QStringLiteral("tottenham"), QStringLiteral("Tottenham Hotspur"),
         QStringLiteral("#132257"), QStringLiteral("#1d3fa0"), {}, {}},
        {QStringLiteral("juventus"), QStringLiteral("Juventus Turin"),
         QStringLiteral("#c6a15b"), QStringLiteral("#e8e8e8"), blk, blk},
        {QStringLiteral("acmilan"), QStringLiteral("AC Mailand"),
         QStringLiteral("#fb090b"), QStringLiteral("#fb090b"), blk, blk},
        {QStringLiteral("intermilan"), QStringLiteral("Inter Mailand"),
         QStringLiteral("#0068a8"), QStringLiteral("#0068a8"), blk, blk},
        {QStringLiteral("napoli"), QStringLiteral("SSC Napoli"),
         QStringLiteral("#12a0d7"), QStringLiteral("#12a0d7"), {}, {}},
        {QStringLiteral("roma"), QStringLiteral("AS Rom"),
         QStringLiteral("#8e1f2f"), QStringLiteral("#f0bc42"), {}, {}},
        {QStringLiteral("psg"), QStringLiteral("Paris Saint-Germain"),
         QStringLiteral("#004170"), QStringLiteral("#da291c"), {}, {}},
        {QStringLiteral("ajax"), QStringLiteral("Ajax Amsterdam"),
         QStringLiteral("#d2122e"), QStringLiteral("#d2122e"), {}, {}},
        {QStringLiteral("benfica"), QStringLiteral("SL Benfica"),
         QStringLiteral("#e30613"), QStringLiteral("#e30613"), {}, {}},
        {QStringLiteral("porto"), QStringLiteral("FC Porto"),
         QStringLiteral("#00428c"), QStringLiteral("#00428c"), {}, {}},
        {QStringLiteral("celtic"), QStringLiteral("Celtic FC"),
         QStringLiteral("#018749"), QStringLiteral("#018749"), {}, {}},
    };
    return themes;
}

QHash<QString, QString> presetThemeSettings(const QString &presetId)
{
    const ClubTheme *theme = &clubThemes().first(); // neutral fallback
    for (const ClubTheme &candidate : clubThemes()) {
        if (candidate.id == presetId) {
            theme = &candidate;
            break;
        }
    }

    const QString headerNight =
        theme->headerNight.isEmpty() ? theme->primary : theme->headerNight;
    const QString headerDay = theme->headerDay.isEmpty() ? theme->primary : theme->headerDay;

    return {
        {QStringLiteral("theme_preset"), theme->id},
        // Night: shared anthracite base + brand accents.
        {QStringLiteral("night_background_color"), QStringLiteral("#262626")},
        {QStringLiteral("night_surface_color"), QStringLiteral("#343434")},
        {QStringLiteral("night_sidebar_color"), QStringLiteral("#1f1f1f")},
        {QStringLiteral("night_header_color"), headerNight},
        {QStringLiteral("night_primary_color"), theme->primary},
        {QStringLiteral("night_interactive_color"), theme->interactive},
        {QStringLiteral("night_text_color"), QStringLiteral("#F5F5F5")},
        // Day: shared light base + brand accents.
        {QStringLiteral("day_background_color"), QStringLiteral("#F0F2F6")},
        {QStringLiteral("day_surface_color"), QStringLiteral("#FFFFFF")},
        {QStringLiteral("day_sidebar_color"), QStringLiteral("#E7E9ED")},
        {QStringLiteral("day_header_color"), headerDay},
        {QStringLiteral("day_primary_color"), theme->primary},
        {QStringLiteral("day_interactive_color"), theme->interactive},
        {QStringLiteral("day_text_color"), QStringLiteral("#31333F")},
    };
}

} // namespace fm
