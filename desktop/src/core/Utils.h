#pragma once

#include <QColor>
#include <QDateTime>
#include <QSet>
#include <QString>

namespace fm {

// --- Parsing helpers, ported 1:1 from legacy/src/utils.py ---

// Sentinel for "Not for Sale" transfer values (matches the legacy 2e9).
inline constexpr double kUnbuyableValue = 2'000'000'000.0;

// "€1.2M" -> 1200000, "€500K - €800K" -> 500000, "Not for Sale" -> 2e9,
// unparseable/empty -> 0.
double valueToFloat(const QString &valueStr);

// "Erling Braut Haaland" -> "Haaland"
QString getLastName(const QString &fullName);

// Accent-/umlaut-insensitive, lowercased search key for fuzzy name matching:
// "Müller"/"Muller" -> "muller", "Håland" -> "haland", "Gießen" -> "giessen".
QString foldForSearch(const QString &text);

// 'AM (RL), ST (C)' -> {"AM (R)", "AM (L)", "ST (C)"}. Handles "D/WB (R)"
// and side-less bases ("DM" stays "DM", bare "ST" becomes "ST (C)").
QSet<QString> parsePositionString(const QString &posStr);

// Parses a DWRS-history timestamp ("yyyy-MM-dd HH:mm:ss") into a QDateTime;
// returns an invalid QDateTime for unparseable/empty input.
QDateTime parseDwrsTimestamp(const QString &timestamp);

// --- Color helpers (WCAG + cell styling), ported from utils.py ---

struct CellStyle {
    QColor background; // invalid if no styling applies
    QColor text;
    bool isValid() const { return background.isValid(); }
};

// WCAG relative luminance (0-1) and contrast ratio (1-21).
double relativeLuminance(const QColor &color);
double contrastRatio(const QColor &a, const QColor &b);

// DWRS (0-100, normalized 30-95) -> gist_rainbow background with smart
// black/white text.
CellStyle dwrsCellStyle(double normalizedValue);

// Attribute (1-20) -> 5-tier color (red/orange/yellow/light green/green).
CellStyle attributeCellStyle(int value);

// Personality category ("good"/"neutral"/"bad") -> traffic-light style;
// unknown/empty -> invalid style.
CellStyle personalityCellStyle(const QString &category);

} // namespace fm
