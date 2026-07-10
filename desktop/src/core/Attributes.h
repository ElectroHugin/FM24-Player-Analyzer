#pragma once

#include <QHash>
#include <QString>
#include <array>
#include <cstdint>

namespace fm {

// The 40 rated 1-20 attributes, in a fixed canonical order. Index into
// Player::attrLo/attrHi. Names are the full names used as DB columns in the
// legacy app (attribute_mapping values), which keeps the migration and the
// DWRS spec readable.
enum class Attr : uint8_t {
    OneVsOne,
    Acceleration,
    AerialReach,
    Aggression,
    Agility,
    Anticipation,
    Balance,
    Bravery,
    CommandOfArea,
    Concentration,
    Composure,
    Corners,
    Crossing,
    Decisions,
    Determination,
    Dribbling,
    Finishing,
    FirstTouch,
    Flair,
    Handling,
    Heading,
    JumpingReach,
    Kicking,
    Leadership,
    LongShots,
    Marking,
    OffTheBall,
    Pace,
    Passing,
    Positioning,
    Reflexes,
    RushingOutTendency,
    Stamina,
    Strength,
    Tackling,
    Teamwork,
    Technique,
    Throwing,
    Vision,
    WorkRate,
    Count // = 40
};

inline constexpr int kAttrCount = static_cast<int>(Attr::Count);

// Full display/DB name for each attribute, indexed by Attr.
const std::array<QString, kAttrCount> &attrNames();

// Full name ("Acceleration") -> Attr index; -1 if not a rated attribute.
int attrIndexByName(const QString &fullName);

inline int idx(Attr a) { return static_cast<int>(a); }

} // namespace fm
