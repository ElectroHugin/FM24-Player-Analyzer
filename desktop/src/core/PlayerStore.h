#pragma once

#include "Player.h"

#include <QHash>
#include <QString>

#include <vector>

namespace fm {

// In-memory store of all players, the single source the UI models and the
// analysis engines read from. Contiguous storage; lookups by uid or db id go
// through hash indexes. Loading/saving lives in Database (M3).
class PlayerStore
{
public:
    PlayerStore() = default;

    // Replaces the whole store (e.g. after a DB load or import). Rebuilds indexes.
    void reset(std::vector<Player> players);

    void clear();

    int size() const { return static_cast<int>(m_players.size()); }
    bool isEmpty() const { return m_players.empty(); }

    const std::vector<Player> &players() const { return m_players; }
    const Player &at(int row) const { return m_players[row]; }
    Player &at(int row) { return m_players[row]; }

    // Row index by FM uid / db id; -1 if unknown.
    int rowByUid(const QString &uid) const { return m_uidIndex.value(uid, -1); }
    int rowById(int id) const { return m_idIndex.value(id, -1); }

    const Player *findByUid(const QString &uid) const;

    // Appends a player and indexes it; returns its row.
    int add(Player player);

    // Removes by row indexes (descending-safe); rebuilds indexes.
    void removeRows(std::vector<int> rows);

private:
    void rebuildIndexes();

    std::vector<Player> m_players;
    QHash<QString, int> m_uidIndex;
    QHash<int, int> m_idIndex;
};

} // namespace fm
