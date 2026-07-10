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
    return true;
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

} // namespace fm
