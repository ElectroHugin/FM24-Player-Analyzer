#pragma once

#include "Player.h"

#include <QStringList>

#include <vector>

namespace fm {

class Database;
class Definitions;

// Port of legacy role_logic.auto_assign_roles_to_unassigned: every player
// without assigned roles receives the default roles derived from his game
// positions (position_to_role_mapping), sorted alphabetically.
namespace RoleAssignment {

// Mutates players in place and persists the changed ones. Returns the uids
// of the players that received roles (empty on no-op); sets errorOut and
// returns an empty list on DB failure with *errorOut non-empty.
QStringList autoAssignRolesToUnassigned(Database &db, std::vector<Player> &players,
                                        const Definitions &definitions,
                                        QString *errorOut = nullptr);

} // namespace RoleAssignment

} // namespace fm
