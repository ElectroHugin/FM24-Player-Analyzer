#include "PlayerStore.h"

#include "Utils.h"

#include <algorithm>

namespace fm {

QSet<QString> Player::parsedPositions() const
{
    return parsePositionString(positionRaw);
}

void PlayerStore::reset(std::vector<Player> players)
{
    m_players = std::move(players);
    rebuildIndexes();
}

void PlayerStore::clear()
{
    m_players.clear();
    m_uidIndex.clear();
    m_idIndex.clear();
}

const Player *PlayerStore::findByUid(const QString &uid) const
{
    const int row = rowByUid(uid);
    return row < 0 ? nullptr : &m_players[row];
}

int PlayerStore::add(Player player)
{
    const int row = static_cast<int>(m_players.size());
    m_uidIndex.insert(player.uid, row);
    if (player.id != 0)
        m_idIndex.insert(player.id, row);
    m_players.push_back(std::move(player));
    return row;
}

void PlayerStore::removeRows(std::vector<int> rows)
{
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (const int row : rows) {
        if (row >= 0 && row < static_cast<int>(m_players.size()))
            m_players.erase(m_players.begin() + row);
    }
    rebuildIndexes();
}

void PlayerStore::rebuildIndexes()
{
    m_uidIndex.clear();
    m_idIndex.clear();
    m_uidIndex.reserve(static_cast<int>(m_players.size()));
    for (int row = 0; row < static_cast<int>(m_players.size()); ++row) {
        m_uidIndex.insert(m_players[row].uid, row);
        if (m_players[row].id != 0)
            m_idIndex.insert(m_players[row].id, row);
    }
}

} // namespace fm
