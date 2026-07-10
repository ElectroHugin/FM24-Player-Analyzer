#include "Constants.h"

#include "Attributes.h"

namespace fm {

const std::array<QString, kAttrCount> &attrNames()
{
    static const std::array<QString, kAttrCount> names = {
        QStringLiteral("One vs One"),
        QStringLiteral("Acceleration"),
        QStringLiteral("Aerial Reach"),
        QStringLiteral("Aggression"),
        QStringLiteral("Agility"),
        QStringLiteral("Anticipation"),
        QStringLiteral("Balance"),
        QStringLiteral("Bravery"),
        QStringLiteral("Command of Area"),
        QStringLiteral("Concentration"),
        QStringLiteral("Composure"),
        QStringLiteral("Corners"),
        QStringLiteral("Crossing"),
        QStringLiteral("Decisions"),
        QStringLiteral("Determination"),
        QStringLiteral("Dribbling"),
        QStringLiteral("Finishing"),
        QStringLiteral("First Touch"),
        QStringLiteral("Flair"),
        QStringLiteral("Handling"),
        QStringLiteral("Heading"),
        QStringLiteral("Jumping Reach"),
        QStringLiteral("Kicking"),
        QStringLiteral("Leadership"),
        QStringLiteral("Long Shots"),
        QStringLiteral("Marking"),
        QStringLiteral("Off the Ball"),
        QStringLiteral("Pace"),
        QStringLiteral("Passing"),
        QStringLiteral("Positioning"),
        QStringLiteral("Reflexes"),
        QStringLiteral("Rushing Out (Tendency)"),
        QStringLiteral("Stamina"),
        QStringLiteral("Strength"),
        QStringLiteral("Tackling"),
        QStringLiteral("Teamwork"),
        QStringLiteral("Technique"),
        QStringLiteral("Throwing"),
        QStringLiteral("Vision"),
        QStringLiteral("Work Rate"),
    };
    return names;
}

int attrIndexByName(const QString &fullName)
{
    static const QHash<QString, int> index = [] {
        QHash<QString, int> h;
        const auto &names = attrNames();
        for (int i = 0; i < kAttrCount; ++i)
            h.insert(names[i], i);
        return h;
    }();
    return index.value(fullName, -1);
}

const QHash<QString, QString> &attributeMapping()
{
    static const QHash<QString, QString> mapping = {
        {QStringLiteral("Reg"), QStringLiteral("Registration")},
        {QStringLiteral("Inf"), QStringLiteral("Information")},
        {QStringLiteral("Name"), QStringLiteral("Name")},
        {QStringLiteral("Age"), QStringLiteral("Age")},
        {QStringLiteral("Wage"), QStringLiteral("Wage")},
        {QStringLiteral("Transfer Value"), QStringLiteral("Transfer Value")},
        {QStringLiteral("Nat"), QStringLiteral("Nationality")},
        {QStringLiteral("2nd Nat"), QStringLiteral("Second Nationality")},
        {QStringLiteral("Position"), QStringLiteral("Position")},
        {QStringLiteral("Personality"), QStringLiteral("Personality")},
        {QStringLiteral("Media Handling"), QStringLiteral("Media Handling")},
        {QStringLiteral("Av Rat"), QStringLiteral("Average Rating")},
        {QStringLiteral("Left Foot"), QStringLiteral("Left Foot")},
        {QStringLiteral("Right Foot"), QStringLiteral("Right Foot")},
        {QStringLiteral("Height"), QStringLiteral("Height")},
        {QStringLiteral("1v1"), QStringLiteral("One vs One")},
        {QStringLiteral("Acc"), QStringLiteral("Acceleration")},
        {QStringLiteral("Aer"), QStringLiteral("Aerial Reach")},
        {QStringLiteral("Agg"), QStringLiteral("Aggression")},
        {QStringLiteral("Agi"), QStringLiteral("Agility")},
        {QStringLiteral("Ant"), QStringLiteral("Anticipation")},
        {QStringLiteral("Bal"), QStringLiteral("Balance")},
        {QStringLiteral("Bra"), QStringLiteral("Bravery")},
        {QStringLiteral("Cmd"), QStringLiteral("Command of Area")},
        {QStringLiteral("Cnt"), QStringLiteral("Concentration")},
        {QStringLiteral("Cmp"), QStringLiteral("Composure")},
        {QStringLiteral("Cro"), QStringLiteral("Crossing")},
        {QStringLiteral("Dec"), QStringLiteral("Decisions")},
        {QStringLiteral("Det"), QStringLiteral("Determination")},
        {QStringLiteral("Dri"), QStringLiteral("Dribbling")},
        {QStringLiteral("Fin"), QStringLiteral("Finishing")},
        {QStringLiteral("Fir"), QStringLiteral("First Touch")},
        {QStringLiteral("Fla"), QStringLiteral("Flair")},
        {QStringLiteral("Han"), QStringLiteral("Handling")},
        {QStringLiteral("Hea"), QStringLiteral("Heading")},
        {QStringLiteral("Jum"), QStringLiteral("Jumping Reach")},
        {QStringLiteral("Kic"), QStringLiteral("Kicking")},
        {QStringLiteral("Ldr"), QStringLiteral("Leadership")},
        {QStringLiteral("Lon"), QStringLiteral("Long Shots")},
        {QStringLiteral("Mar"), QStringLiteral("Marking")},
        {QStringLiteral("OtB"), QStringLiteral("Off the Ball")},
        {QStringLiteral("Pac"), QStringLiteral("Pace")},
        {QStringLiteral("Pas"), QStringLiteral("Passing")},
        {QStringLiteral("Pos"), QStringLiteral("Positioning")},
        {QStringLiteral("Ref"), QStringLiteral("Reflexes")},
        {QStringLiteral("Sta"), QStringLiteral("Stamina")},
        {QStringLiteral("Str"), QStringLiteral("Strength")},
        {QStringLiteral("Tck"), QStringLiteral("Tackling")},
        {QStringLiteral("Tea"), QStringLiteral("Teamwork")},
        {QStringLiteral("Tec"), QStringLiteral("Technique")},
        {QStringLiteral("Thr"), QStringLiteral("Throwing")},
        {QStringLiteral("TRO"), QStringLiteral("Rushing Out (Tendency)")},
        {QStringLiteral("Vis"), QStringLiteral("Vision")},
        {QStringLiteral("Wor"), QStringLiteral("Work Rate")},
        {QStringLiteral("UID"), QStringLiteral("Unique ID")},
        {QStringLiteral("Cor"), QStringLiteral("Corners")},
        {QStringLiteral("Club"), QStringLiteral("Club")},
        {QStringLiteral("Agreed Playing Time"), QStringLiteral("Agreed Playing Time")},
        {QStringLiteral("Preferred Foot"), QStringLiteral("Preferred Foot")},
    };
    return mapping;
}

const QHash<QString, QString> &globalStatCategories()
{
    static const QHash<QString, QString> categories = {
        {QStringLiteral("Pace"), QStringLiteral("Extremely Important")},
        {QStringLiteral("Acceleration"), QStringLiteral("Extremely Important")},
        {QStringLiteral("Jumping Reach"), QStringLiteral("Important")},
        {QStringLiteral("Anticipation"), QStringLiteral("Important")},
        {QStringLiteral("Balance"), QStringLiteral("Important")},
        {QStringLiteral("Agility"), QStringLiteral("Important")},
        {QStringLiteral("Concentration"), QStringLiteral("Important")},
        {QStringLiteral("Finishing"), QStringLiteral("Important")},
        {QStringLiteral("Work Rate"), QStringLiteral("Good")},
        {QStringLiteral("Dribbling"), QStringLiteral("Good")},
        {QStringLiteral("Stamina"), QStringLiteral("Good")},
        {QStringLiteral("Strength"), QStringLiteral("Good")},
        {QStringLiteral("Passing"), QStringLiteral("Good")},
        {QStringLiteral("Determination"), QStringLiteral("Good")},
        {QStringLiteral("Vision"), QStringLiteral("Good")},
        {QStringLiteral("Long Shots"), QStringLiteral("Decent")},
        {QStringLiteral("Marking"), QStringLiteral("Decent")},
        {QStringLiteral("Decisions"), QStringLiteral("Decent")},
        {QStringLiteral("First Touch"), QStringLiteral("Decent")},
        {QStringLiteral("Off the Ball"), QStringLiteral("Almost Irrelevant")},
        {QStringLiteral("Tackling"), QStringLiteral("Almost Irrelevant")},
        {QStringLiteral("Teamwork"), QStringLiteral("Almost Irrelevant")},
        {QStringLiteral("Composure"), QStringLiteral("Almost Irrelevant")},
        {QStringLiteral("Technique"), QStringLiteral("Almost Irrelevant")},
        {QStringLiteral("Positioning"), QStringLiteral("Almost Irrelevant")},
    };
    return categories;
}

const QHash<QString, QString> &gkStatCategories()
{
    static const QHash<QString, QString> categories = {
        {QStringLiteral("Agility"), QStringLiteral("Top Importance")},
        {QStringLiteral("Aerial Reach"), QStringLiteral("High Importance")},
        {QStringLiteral("Reflexes"), QStringLiteral("High Importance")},
        {QStringLiteral("Command of Area"), QStringLiteral("Medium Importance")},
        {QStringLiteral("Handling"), QStringLiteral("Medium Importance")},
        {QStringLiteral("One vs One"), QStringLiteral("Medium Importance")},
    };
    return categories;
}

const QHash<QString, double> &weightDefaults()
{
    static const QHash<QString, double> defaults = {
        {QStringLiteral("Extremely Important"), 8.0},
        {QStringLiteral("Important"), 4.0},
        {QStringLiteral("Good"), 2.0},
        {QStringLiteral("Decent"), 1.0},
        {QStringLiteral("Almost Irrelevant"), 0.2},
    };
    return defaults;
}

const QHash<QString, double> &gkWeightDefaults()
{
    static const QHash<QString, double> defaults = {
        {QStringLiteral("Top Importance"), 10.0},
        {QStringLiteral("High Importance"), 8.0},
        {QStringLiteral("Medium Importance"), 6.0},
        {QStringLiteral("Key"), 4.0},
        {QStringLiteral("Preferable"), 2.0},
        {QStringLiteral("Other"), 0.5},
    };
    return defaults;
}

const QHash<QString, QString> &personalityDefaults()
{
    static const QHash<QString, QString> defaults = {
        // Good (positive)
        {QStringLiteral("Model Citizen"), QStringLiteral("good")},
        {QStringLiteral("Perfectionist"), QStringLiteral("good")},
        {QStringLiteral("Resolute"), QStringLiteral("good")},
        {QStringLiteral("Model Professional"), QStringLiteral("good")},
        {QStringLiteral("Professional"), QStringLiteral("good")},
        {QStringLiteral("Fairly Professional"), QStringLiteral("good")},
        {QStringLiteral("Spirited"), QStringLiteral("good")},
        {QStringLiteral("Driven"), QStringLiteral("good")},
        {QStringLiteral("Determined"), QStringLiteral("good")},
        {QStringLiteral("Fairly Determined"), QStringLiteral("good")},
        {QStringLiteral("Iron Willed"), QStringLiteral("good")},
        {QStringLiteral("Resilient"), QStringLiteral("good")},
        {QStringLiteral("Charismatic Leader"), QStringLiteral("good")},
        {QStringLiteral("Born Leader"), QStringLiteral("good")},
        {QStringLiteral("Leader"), QStringLiteral("good")},
        {QStringLiteral("Very Ambitious"), QStringLiteral("good")},
        {QStringLiteral("Ambitious"), QStringLiteral("good")},
        {QStringLiteral("Fairly Ambitious"), QStringLiteral("good")},
        {QStringLiteral("Fickle"), QStringLiteral("good")},
        {QStringLiteral("Mercenary"), QStringLiteral("good")},
        // Bad (negative)
        {QStringLiteral("Slack"), QStringLiteral("bad")},
        {QStringLiteral("Casual"), QStringLiteral("bad")},
        {QStringLiteral("Temperamental"), QStringLiteral("bad")},
        {QStringLiteral("Easily Discouraged"), QStringLiteral("bad")},
        {QStringLiteral("Low Determination"), QStringLiteral("bad")},
        {QStringLiteral("Spineless"), QStringLiteral("bad")},
        {QStringLiteral("Low Self-Belief"), QStringLiteral("bad")},
        {QStringLiteral("Unambitious"), QStringLiteral("bad")},
        // Neutral
        {QStringLiteral("Jovial"), QStringLiteral("neutral")},
        {QStringLiteral("Light-Hearted"), QStringLiteral("neutral")},
        {QStringLiteral("Devoted"), QStringLiteral("neutral")},
        {QStringLiteral("Very Loyal"), QStringLiteral("neutral")},
        {QStringLiteral("Loyal"), QStringLiteral("neutral")},
        {QStringLiteral("Fairly Loyal"), QStringLiteral("neutral")},
        {QStringLiteral("Honest"), QStringLiteral("neutral")},
        {QStringLiteral("Sporting"), QStringLiteral("neutral")},
        {QStringLiteral("Fairly Sporting"), QStringLiteral("neutral")},
        {QStringLiteral("Unsporting"), QStringLiteral("neutral")},
        {QStringLiteral("Realist"), QStringLiteral("neutral")},
        {QStringLiteral("Balanced"), QStringLiteral("neutral")},
    };
    return defaults;
}

const QStringList &fieldPlayerAptOptions()
{
    static const QStringList options = {
        QStringLiteral("None"), QStringLiteral("Star Player"), QStringLiteral("Important Player"),
        QStringLiteral("Regular Starter"), QStringLiteral("Squad Player"), QStringLiteral("Impact Sub"),
        QStringLiteral("Fringe Player"), QStringLiteral("Breakthrough Prospect"),
        QStringLiteral("Future Prospect"), QStringLiteral("Youngster"), QStringLiteral("Emergency Backup"),
        QStringLiteral("2nd Team Regular"), QStringLiteral("Surplus to Requirements"),
    };
    return options;
}

const QStringList &gkAptOptions()
{
    static const QStringList options = {
        QStringLiteral("None"), QStringLiteral("Star Player"), QStringLiteral("Important Player"),
        QStringLiteral("First-Choice Goalkeeper"), QStringLiteral("Cup Goalkeeper"),
        QStringLiteral("Backup"), QStringLiteral("Breakthrough Prospect"), QStringLiteral("Future Prospect"),
        QStringLiteral("Youngster"), QStringLiteral("Emergency Backup"),
        QStringLiteral("Surplus to Requirements"),
    };
    return options;
}

const QHash<QString, QString> &aptAbbreviations()
{
    static const QHash<QString, QString> abbreviations = {
        {QStringLiteral("Important Player"), QStringLiteral("Important")},
        {QStringLiteral("Breakthrough Prospect"), QStringLiteral("Prospect")},
        {QStringLiteral("Future Prospect"), QStringLiteral("Future")},
        {QStringLiteral("Emergency Backup"), QStringLiteral("Backup")},
        {QStringLiteral("2nd Team Regular"), QStringLiteral("2nd Team")},
        {QStringLiteral("Surplus to Requirements"), QStringLiteral("Surplus")},
    };
    return abbreviations;
}

const QHash<QString, SlotPosition> &masterPositionMap()
{
    static const QHash<QString, SlotPosition> map = {
        // Defense (5 slots, indices 0-4)
        {QStringLiteral("DL"), {QStringLiteral("Defense"), 0}},
        {QStringLiteral("WBL"), {QStringLiteral("Defense"), 0}},
        {QStringLiteral("DCL"), {QStringLiteral("Defense"), 1}},
        {QStringLiteral("DC"), {QStringLiteral("Defense"), 2}},
        {QStringLiteral("DCR"), {QStringLiteral("Defense"), 3}},
        {QStringLiteral("DR"), {QStringLiteral("Defense"), 4}},
        {QStringLiteral("WBR"), {QStringLiteral("Defense"), 4}},
        // Defensive Midfield
        {QStringLiteral("DML"), {QStringLiteral("Defensive Midfield"), 0}},
        {QStringLiteral("DMCL"), {QStringLiteral("Defensive Midfield"), 1}},
        {QStringLiteral("DMC"), {QStringLiteral("Defensive Midfield"), 2}},
        {QStringLiteral("DMCR"), {QStringLiteral("Defensive Midfield"), 3}},
        {QStringLiteral("DMR"), {QStringLiteral("Defensive Midfield"), 4}},
        // Midfield
        {QStringLiteral("ML"), {QStringLiteral("Midfield"), 0}},
        {QStringLiteral("MCL"), {QStringLiteral("Midfield"), 1}},
        {QStringLiteral("MC"), {QStringLiteral("Midfield"), 2}},
        {QStringLiteral("MCR"), {QStringLiteral("Midfield"), 3}},
        {QStringLiteral("MR"), {QStringLiteral("Midfield"), 4}},
        // Attacking Midfield
        {QStringLiteral("AML"), {QStringLiteral("Attacking Midfield"), 0}},
        {QStringLiteral("AMCL"), {QStringLiteral("Attacking Midfield"), 1}},
        {QStringLiteral("AMC"), {QStringLiteral("Attacking Midfield"), 2}},
        {QStringLiteral("AMCR"), {QStringLiteral("Attacking Midfield"), 3}},
        {QStringLiteral("AMR"), {QStringLiteral("Attacking Midfield"), 4}},
        // Strikers (3 slots, indices 0-2); ST and STC share the central slot
        {QStringLiteral("STL"), {QStringLiteral("Strikers"), 0}},
        {QStringLiteral("STC"), {QStringLiteral("Strikers"), 1}},
        {QStringLiteral("ST"), {QStringLiteral("Strikers"), 1}},
        {QStringLiteral("STR"), {QStringLiteral("Strikers"), 2}},
    };
    return map;
}

const QHash<QString, QStringList> &tacticalSlotToGamePositions()
{
    static const QHash<QString, QStringList> map = {
        {QStringLiteral("GK"), {QStringLiteral("GK")}},
        // Defense
        {QStringLiteral("DL"), {QStringLiteral("D (L)"), QStringLiteral("WB (L)")}},
        {QStringLiteral("WBL"), {QStringLiteral("WB (L)")}},
        {QStringLiteral("DCL"), {QStringLiteral("D (C)")}},
        {QStringLiteral("DC"), {QStringLiteral("D (C)")}},
        {QStringLiteral("DCR"), {QStringLiteral("D (C)")}},
        {QStringLiteral("DR"), {QStringLiteral("D (R)"), QStringLiteral("WB (R)")}},
        {QStringLiteral("WBR"), {QStringLiteral("WB (R)")}},
        // Defensive Midfield
        {QStringLiteral("DML"), {QStringLiteral("WB (L)")}},
        {QStringLiteral("DMCL"), {QStringLiteral("DM")}},
        {QStringLiteral("DMC"), {QStringLiteral("DM")}},
        {QStringLiteral("DMCR"), {QStringLiteral("DM")}},
        {QStringLiteral("DMR"), {QStringLiteral("WB (R)")}},
        // Midfield
        {QStringLiteral("ML"), {QStringLiteral("M (L)")}},
        {QStringLiteral("MCL"), {QStringLiteral("M (C)")}},
        {QStringLiteral("MC"), {QStringLiteral("M (C)")}},
        {QStringLiteral("MCR"), {QStringLiteral("M (C)")}},
        {QStringLiteral("MR"), {QStringLiteral("M (R)")}},
        // Attacking Midfield
        {QStringLiteral("AML"), {QStringLiteral("AM (L)")}},
        {QStringLiteral("AMCL"), {QStringLiteral("AM (C)")}},
        {QStringLiteral("AMC"), {QStringLiteral("AM (C)")}},
        {QStringLiteral("AMCR"), {QStringLiteral("AM (C)")}},
        {QStringLiteral("AMR"), {QStringLiteral("AM (R)")}},
        // Strikers: all striker slots map to the central striker game position
        {QStringLiteral("STL"), {QStringLiteral("ST (C)")}},
        {QStringLiteral("STC"), {QStringLiteral("ST (C)")}},
        {QStringLiteral("ST"), {QStringLiteral("ST (C)")}},
        {QStringLiteral("STR"), {QStringLiteral("ST (C)")}},
    };
    return map;
}

const QHash<QString, int> &stratumOrder()
{
    static const QHash<QString, int> order = {
        {QStringLiteral("GK"), 0},
        {QStringLiteral("Defense"), 1},
        {QStringLiteral("Defensive Midfield"), 2},
        {QStringLiteral("Midfield"), 3},
        {QStringLiteral("Attacking Midfield"), 4},
        {QStringLiteral("Strikers"), 5},
    };
    return order;
}

} // namespace fm
