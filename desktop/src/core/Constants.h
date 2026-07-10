#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace fm {

// --- Static domain constants ported 1:1 from legacy/src/constants.py ---
// Dynamic definition accessors (roles, tactics, ...) live in Definitions.

// FM HTML export header abbreviation -> full column name (~59 entries).
// The schema contract between HTML exports and the database.
const QHash<QString, QString> &attributeMapping();

// DWRS importance category per attribute (field players / goalkeepers).
// Key: full attribute name, value: category name.
const QHash<QString, QString> &globalStatCategories();
const QHash<QString, QString> &gkStatCategories();

// Default category weights (overridable via config.ini).
const QHash<QString, double> &weightDefaults();     // field players
const QHash<QString, double> &gkWeightDefaults();   // goalkeepers

// Default role multipliers.
inline constexpr double kKeyMultiplierDefault = 1.5;
inline constexpr double kPreferableMultiplierDefault = 1.2;

// Personality name -> "good" | "neutral" | "bad" (defaults; definitions.json
// may override).
const QHash<QString, QString> &personalityDefaults();

// Agreed Playing Time options.
const QStringList &fieldPlayerAptOptions();
const QStringList &gkAptOptions();
const QHash<QString, QString> &aptAbbreviations();

// Tactical geometry: slot key -> (stratum name, column index).
struct SlotPosition {
    QString stratum;
    int column = 0;
};
const QHash<QString, SlotPosition> &masterPositionMap();

// Tactical slot -> game positions eligible to fill it (e.g. "DL" -> ["D (L)", "WB (L)"]).
const QHash<QString, QStringList> &tacticalSlotToGamePositions();

// Vertical ordering of strata on the pitch (GK=0 ... Strikers=5).
const QHash<QString, int> &stratumOrder();

} // namespace fm
