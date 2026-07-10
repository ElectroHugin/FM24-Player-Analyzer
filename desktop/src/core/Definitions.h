#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fm {

// Role key/preferable attribute lists (role_specific_weights entry).
struct RoleWeights {
    QStringList key;
    QStringList preferable;
};

// Loads, queries and saves config/definitions.json — the user-editable domain
// data: roles, role weights, position->role mapping, tactics, personalities.
// Port of legacy definitions_loader.py + definitions_handler.py + the dynamic
// accessors in constants.py.
class Definitions
{
public:
    Definitions() = default;

    // Loads from the given JSON file. Returns false and sets errorString() on
    // missing/invalid file.
    bool load(const QString &filePath);

    // Saves back to the file given to load(). Creates a .bak first, restores
    // it on failure (same semantics as legacy save_definitions).
    bool save();

    QString errorString() const { return m_error; }
    QString filePath() const { return m_filePath; }

    // --- Accessors (mirroring constants.py dynamic getters) ---

    // Category ("Goalkeepers", "Defense", ...) -> {abbr -> "Full Name (Duty)"}.
    QHash<QString, QHash<QString, QString>> playerRoles() const;

    // Fixed display order of role categories.
    static QStringList roleCategoryOrder();

    // All role abbreviations, sorted (get_valid_roles).
    QStringList validRoles() const;

    // Goalkeeper role abbreviations (from the "Goalkeepers" category, with the
    // legacy fallback list).
    QStringList gkRoles() const;

    // Role abbr -> key/preferable attribute name lists.
    RoleWeights roleWeights(const QString &role) const;
    QStringList rolesWithWeights() const;

    // Game position ("D (C)") -> eligible role abbreviations.
    QHash<QString, QStringList> positionToRoleMapping() const;

    // Tactic name -> {slot -> role abbr}.
    QHash<QString, QHash<QString, QString>> tacticRoles() const;
    QStringList tacticNames() const;

    // Tactic name -> {stratum -> [slots]}.
    QHash<QString, QHash<QString, QStringList>> tacticLayouts() const;

    // Personality -> "good" | "neutral" | "bad" (definitions override or
    // built-in defaults).
    QHash<QString, QString> personalities() const;

    // Category for a personality string; case-/whitespace-insensitive
    // fallback; "" if unknown (get_personality_category).
    QString personalityCategory(const QString &name) const;

    // Role abbr -> full display name (across all categories).
    QHash<QString, QString> roleDisplayMap() const;

    // Raw access for editing (New Role / New Tactic pages).
    QJsonObject root() const { return m_root; }
    void setRoot(const QJsonObject &root) { m_root = root; }

private:
    QJsonObject m_root;
    QString m_filePath;
    QString m_error;
};

} // namespace fm
