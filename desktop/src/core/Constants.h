#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace fm {

// --- Static domain constants ported 1:1 from legacy/src/constants.py ---
// Dynamic definition accessors (roles, tactics, ...) live in Definitions.

// A supported Football Manager release, identified by a stable id stored in
// the per-database settings ("fm_version").
struct FmDataVersion {
    QString id;          // stable settings key, e.g. "fm24"
    QString displayName; // e.g. "Football Manager 2024"
    bool supported;      // false = shown in the picker but not yet importable
};

// All FM releases the app knows how to import (newest handling first). Add a
// version here plus its mapping in attributeMapping() to support a new game.
const QList<FmDataVersion> &fmDataVersions();
QString defaultFmVersionId();

// FM HTML export header abbreviation -> full column name (~59 entries) for the
// given FM version; the schema contract between HTML exports and the database.
// Unknown/empty ids fall back to the default version.
const QHash<QString, QString> &attributeMapping(const QString &versionId);
// Convenience overload for the default version.
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
