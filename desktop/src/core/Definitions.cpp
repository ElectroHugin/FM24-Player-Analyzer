#include "Definitions.h"

#include "Constants.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <algorithm>

namespace fm {

bool Definitions::load(const QString &filePath)
{
    m_filePath = filePath;
    m_error.clear();

    QFile file(filePath);
    if (!file.exists()) {
        m_error = QStringLiteral("Definitions file not found: %1").arg(filePath);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        m_error = QStringLiteral("Invalid JSON in %1: %2")
                      .arg(filePath, parseError.errorString());
        return false;
    }
    if (!doc.isObject()) {
        m_error = QStringLiteral("Definitions root must be a JSON object: %1").arg(filePath);
        return false;
    }

    m_root = doc.object();

    file.seek(0);
    indexTacticOrder(file.readAll());
    return true;
}

void Definitions::indexTacticOrder(const QByteArray &json)
{
    // Minimal order-preserving scan of the "tactic_roles" object: records the
    // file order of tactic names and of each tactic's slot keys, which
    // QJsonObject discards (it sorts keys). Handles strings/escapes and
    // nesting; definitions.json is machine-written, so this stays simple.
    m_tacticOrder.clear();
    m_slotOrder.clear();

    const QString text = QString::fromUtf8(json);
    const int anchor = text.indexOf(QStringLiteral("\"tactic_roles\""));
    if (anchor < 0)
        return;
    int i = text.indexOf(QLatin1Char('{'), anchor);
    if (i < 0)
        return;

    int depth = 0;
    QString currentTactic;
    bool inString = false;
    QString stringValue;
    for (; i < text.size(); ++i) {
        const QChar c = text[i];
        if (inString) {
            if (c == QLatin1Char('\\') && i + 1 < text.size()) {
                stringValue.append(text[i + 1]);
                ++i;
            } else if (c == QLatin1Char('"')) {
                inString = false;
                // A string at depth 1/2 followed by ':' is a key.
                int j = i + 1;
                while (j < text.size() && text[j].isSpace())
                    ++j;
                const bool isKey = j < text.size() && text[j] == QLatin1Char(':');
                if (isKey && depth == 1) {
                    currentTactic = stringValue;
                    m_tacticOrder.append(stringValue);
                } else if (isKey && depth == 2) {
                    m_slotOrder[currentTactic].append(stringValue);
                }
            } else {
                stringValue.append(c);
            }
            continue;
        }
        if (c == QLatin1Char('"')) {
            inString = true;
            stringValue.clear();
        } else if (c == QLatin1Char('{')) {
            ++depth;
        } else if (c == QLatin1Char('}')) {
            --depth;
            if (depth == 0)
                break; // end of tactic_roles object
        }
    }
}

QStringList Definitions::tacticNamesOrdered() const
{
    return m_tacticOrder.isEmpty() ? tacticNames() : m_tacticOrder;
}

QStringList Definitions::tacticSlotOrder(const QString &tactic) const
{
    const QStringList order = m_slotOrder.value(tactic);
    if (!order.isEmpty())
        return order;
    return m_root.value(QLatin1String("tactic_roles")).toObject().value(tactic).toObject().keys();
}

bool Definitions::save()
{
    m_error.clear();
    if (m_filePath.isEmpty()) {
        m_error = QStringLiteral("No file path set; call load() first.");
        return false;
    }

    const QString backupPath = m_filePath + QStringLiteral(".bak");

    // 1. Back up the current file (if it exists).
    QFile::remove(backupPath);
    const bool hadOriginal = QFile::exists(m_filePath);
    if (hadOriginal && !QFile::copy(m_filePath, backupPath)) {
        m_error = QStringLiteral("Could not create backup %1").arg(backupPath);
        return false;
    }

    // 2. Write the new data.
    QFile file(m_filePath);
    const QByteArray json = QJsonDocument(m_root).toJson(QJsonDocument::Indented);
    const bool written = file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                         && file.write(json) == json.size();
    file.close();

    if (!written) {
        // 3. Restore from backup on failure.
        if (hadOriginal) {
            QFile::remove(m_filePath);
            QFile::copy(backupPath, m_filePath);
        }
        m_error = QStringLiteral("Write failed for %1: %2. Restored from backup.")
                      .arg(m_filePath, file.errorString());
        return false;
    }

    // 4. Success: drop the backup.
    QFile::remove(backupPath);
    return true;
}

QHash<QString, QHash<QString, QString>> Definitions::playerRoles() const
{
    QHash<QString, QHash<QString, QString>> result;
    const QJsonObject categories = m_root.value(QLatin1String("player_roles")).toObject();
    for (auto catIt = categories.begin(); catIt != categories.end(); ++catIt) {
        QHash<QString, QString> roles;
        const QJsonObject roleObj = catIt.value().toObject();
        for (auto roleIt = roleObj.begin(); roleIt != roleObj.end(); ++roleIt)
            roles.insert(roleIt.key(), roleIt.value().toString());
        result.insert(catIt.key(), roles);
    }
    return result;
}

QStringList Definitions::roleCategoryOrder()
{
    return {QStringLiteral("Goalkeepers"), QStringLiteral("Defense"),
            QStringLiteral("Midfield"), QStringLiteral("Attack")};
}

QStringList Definitions::validRoles() const
{
    QStringList roles;
    const QJsonObject categories = m_root.value(QLatin1String("player_roles")).toObject();
    for (auto catIt = categories.begin(); catIt != categories.end(); ++catIt) {
        const QJsonObject roleObj = catIt.value().toObject();
        for (auto roleIt = roleObj.begin(); roleIt != roleObj.end(); ++roleIt)
            roles.append(roleIt.key());
    }
    std::sort(roles.begin(), roles.end());
    return roles;
}

QStringList Definitions::gkRoles() const
{
    QStringList roles;
    const QJsonObject gk = m_root.value(QLatin1String("player_roles"))
                               .toObject()
                               .value(QLatin1String("Goalkeepers"))
                               .toObject();
    for (auto it = gk.begin(); it != gk.end(); ++it)
        roles.append(it.key());
    if (roles.isEmpty())
        roles = {QStringLiteral("GK-D"), QStringLiteral("SK-D"), QStringLiteral("SK-S"),
                 QStringLiteral("SK-A")};
    return roles;
}

RoleWeights Definitions::roleWeights(const QString &role) const
{
    RoleWeights weights;
    const QJsonObject entry = m_root.value(QLatin1String("role_specific_weights"))
                                  .toObject()
                                  .value(role)
                                  .toObject();
    const QJsonArray key = entry.value(QLatin1String("key")).toArray();
    for (const QJsonValue &v : key)
        weights.key.append(v.toString());
    const QJsonArray preferable = entry.value(QLatin1String("preferable")).toArray();
    for (const QJsonValue &v : preferable)
        weights.preferable.append(v.toString());
    return weights;
}

QStringList Definitions::rolesWithWeights() const
{
    return m_root.value(QLatin1String("role_specific_weights")).toObject().keys();
}

QHash<QString, QStringList> Definitions::positionToRoleMapping() const
{
    QHash<QString, QStringList> result;
    const QJsonObject mapping = m_root.value(QLatin1String("position_to_role_mapping")).toObject();
    for (auto it = mapping.begin(); it != mapping.end(); ++it) {
        QStringList roles;
        const QJsonArray arr = it.value().toArray();
        for (const QJsonValue &v : arr)
            roles.append(v.toString());
        result.insert(it.key(), roles);
    }
    return result;
}

QHash<QString, QHash<QString, QString>> Definitions::tacticRoles() const
{
    QHash<QString, QHash<QString, QString>> result;
    const QJsonObject tactics = m_root.value(QLatin1String("tactic_roles")).toObject();
    for (auto tacticIt = tactics.begin(); tacticIt != tactics.end(); ++tacticIt) {
        QHash<QString, QString> slotMap;
        const QJsonObject slotObj = tacticIt.value().toObject();
        for (auto slotIt = slotObj.begin(); slotIt != slotObj.end(); ++slotIt)
            slotMap.insert(slotIt.key(), slotIt.value().toString());
        result.insert(tacticIt.key(), slotMap);
    }
    return result;
}

QStringList Definitions::tacticNames() const
{
    return m_root.value(QLatin1String("tactic_roles")).toObject().keys();
}

QHash<QString, QHash<QString, QStringList>> Definitions::tacticLayouts() const
{
    QHash<QString, QHash<QString, QStringList>> result;
    const QJsonObject layouts = m_root.value(QLatin1String("tactic_layouts")).toObject();
    for (auto tacticIt = layouts.begin(); tacticIt != layouts.end(); ++tacticIt) {
        QHash<QString, QStringList> strata;
        const QJsonObject stratumObj = tacticIt.value().toObject();
        for (auto stratumIt = stratumObj.begin(); stratumIt != stratumObj.end(); ++stratumIt) {
            QStringList slotList;
            const QJsonArray arr = stratumIt.value().toArray();
            for (const QJsonValue &v : arr)
                slotList.append(v.toString());
            strata.insert(stratumIt.key(), slotList);
        }
        result.insert(tacticIt.key(), strata);
    }
    return result;
}

QHash<QString, QString> Definitions::personalities() const
{
    if (!m_root.contains(QLatin1String("personalities")))
        return personalityDefaults();

    QHash<QString, QString> result;
    const QJsonObject obj = m_root.value(QLatin1String("personalities")).toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        result.insert(it.key(), it.value().toString());
    return result;
}

QString Definitions::personalityCategory(const QString &name) const
{
    if (name.isEmpty())
        return QString();
    const QHash<QString, QString> table = personalities();
    const auto exact = table.constFind(name);
    if (exact != table.constEnd())
        return exact.value();
    const QString low = name.trimmed().toLower();
    for (auto it = table.constBegin(); it != table.constEnd(); ++it) {
        if (it.key().toLower() == low)
            return it.value();
    }
    return QString();
}

QHash<QString, QString> Definitions::roleDisplayMap() const
{
    QHash<QString, QString> result;
    const QJsonObject categories = m_root.value(QLatin1String("player_roles")).toObject();
    for (auto catIt = categories.begin(); catIt != categories.end(); ++catIt) {
        const QJsonObject roleObj = catIt.value().toObject();
        for (auto roleIt = roleObj.begin(); roleIt != roleObj.end(); ++roleIt)
            result.insert(roleIt.key(), roleIt.value().toString());
    }
    return result;
}

QHash<QString, QPair<int, int>> Definitions::naturalRoleSorter() const
{
    // role -> game positions it can play (reverse of position_to_role_mapping).
    QHash<QString, QStringList> roleToPositions;
    const auto posMap = positionToRoleMapping();
    for (auto it = posMap.constBegin(); it != posMap.constEnd(); ++it) {
        for (const QString &role : it.value())
            roleToPositions[role].append(it.key());
    }

    QHash<QString, QPair<int, int>> sorter;
    const auto &slotMap = masterPositionMap();
    const auto &slotPositions = tacticalSlotToGamePositions();
    const auto &strata = stratumOrder();

    for (const QString &role : validRoles()) {
        if (role.contains(QLatin1String("GK")) || role.contains(QLatin1String("SK"))) {
            sorter.insert(role, {0, 0});
            continue;
        }
        const QStringList positions = roleToPositions.value(role);
        if (positions.isEmpty()) {
            sorter.insert(role, {99, 99});
            continue;
        }
        // Highest (most attacking) slot the role can occupy; ties break left.
        QPair<int, int> best(-1, -1);
        for (auto slotIt = slotMap.constBegin(); slotIt != slotMap.constEnd(); ++slotIt) {
            const QStringList validGamePositions = slotPositions.value(slotIt.key());
            bool matches = false;
            for (const QString &position : positions) {
                if (validGamePositions.contains(position)) {
                    matches = true;
                    break;
                }
            }
            if (!matches)
                continue;
            const int stratumScore = strata.value(slotIt.value().stratum, 99);
            const QPair<int, int> current(stratumScore, slotIt.value().column);
            if (current.first > best.first
                || (current.first == best.first && current.second < best.second)) {
                best = current;
            }
        }
        sorter.insert(role, best.first < 0 ? QPair<int, int>(99, 99) : best);
    }
    return sorter;
}

QStringList Definitions::sortRolesNaturally(QStringList roles) const
{
    const auto sorter = naturalRoleSorter();
    std::stable_sort(roles.begin(), roles.end(), [&sorter](const QString &a, const QString &b) {
        return sorter.value(a, {99, 99}) < sorter.value(b, {99, 99});
    });
    return roles;
}

} // namespace fm
