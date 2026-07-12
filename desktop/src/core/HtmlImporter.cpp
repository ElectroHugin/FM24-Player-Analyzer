#include "HtmlImporter.h"

#include "Attributes.h"
#include "Constants.h"
#include "Database.h"
#include "LegacyMigrator.h"
#include "Utils.h"

#include <QFile>
#include <QHash>
#include <QSet>

namespace fm {

namespace {

constexpr int kUpsertBatchSize = 2000;

// Columns FM routinely exports that the app intentionally ignores (legacy
// IGNORED_EXPORT_COLUMNS) — listed so the unknown-column warning does not cry
// wolf on every normal export.
const QSet<QString> &ignoredExportColumns()
{
    static const QSet<QString> ignored = {
        QStringLiteral("CON"), QStringLiteral("Ability"),
        QStringLiteral("Position/Role/Duty"), QStringLiteral("Pun")};
    return ignored;
}

// Decodes the handful of HTML entities FM exports actually contain.
QString decodeEntities(const QString &text)
{
    if (!text.contains(QLatin1Char('&')))
        return text;

    QString out;
    out.reserve(text.size());
    int i = 0;
    const int n = text.size();
    while (i < n) {
        const QChar ch = text.at(i);
        if (ch != QLatin1Char('&')) {
            out.append(ch);
            ++i;
            continue;
        }
        const int semi = text.indexOf(QLatin1Char(';'), i + 1);
        if (semi < 0 || semi - i > 10) { // not an entity
            out.append(ch);
            ++i;
            continue;
        }
        const QString entity = text.mid(i + 1, semi - i - 1);
        if (entity == QLatin1String("amp")) {
            out.append(QLatin1Char('&'));
        } else if (entity == QLatin1String("lt")) {
            out.append(QLatin1Char('<'));
        } else if (entity == QLatin1String("gt")) {
            out.append(QLatin1Char('>'));
        } else if (entity == QLatin1String("quot")) {
            out.append(QLatin1Char('"'));
        } else if (entity == QLatin1String("apos")) {
            out.append(QLatin1Char('\''));
        } else if (entity == QLatin1String("nbsp")) {
            out.append(QLatin1Char(' '));
        } else if (entity.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            const uint code = entity.startsWith(QLatin1String("#x"), Qt::CaseInsensitive)
                                  ? entity.mid(2).toUInt(&ok, 16)
                                  : entity.mid(1).toUInt(&ok, 10);
            if (ok && code > 0)
                out.append(QChar(code));
            else
                out.append(text.mid(i, semi - i + 1));
        } else {
            out.append(text.mid(i, semi - i + 1));
            i = semi + 1;
            continue;
        }
        i = semi + 1;
    }
    return out;
}

// Text content of an HTML fragment: nested tags stripped, entities decoded,
// whitespace trimmed.
QString cellText(const QString &html, int from, int to)
{
    QString out;
    out.reserve(to - from);
    bool inTag = false;
    for (int i = from; i < to; ++i) {
        const QChar ch = html.at(i);
        if (inTag) {
            if (ch == QLatin1Char('>'))
                inTag = false;
        } else if (ch == QLatin1Char('<')) {
            inTag = true;
        } else {
            out.append(ch);
        }
    }
    return decodeEntities(out).trimmed();
}

// Finds the next "<tag" (case-insensitive) at a tag boundary from pos.
// Returns the index of '<', or -1.
int findTag(const QString &html, const QString &tag, int pos)
{
    const QString needle = QLatin1Char('<') + tag;
    while (true) {
        const int at = html.indexOf(needle, pos, Qt::CaseInsensitive);
        if (at < 0)
            return -1;
        const int after = at + needle.size();
        if (after >= html.size())
            return -1;
        const QChar next = html.at(after);
        if (next == QLatin1Char('>') || next.isSpace() || next == QLatin1Char('/'))
            return at;
        pos = at + 1;
    }
}

// True if the player uid is a newgen id ("r-" prefix).
bool isNewgenUid(const QString &uid)
{
    return uid.startsWith(QLatin1String("r-"));
}

} // namespace

bool HtmlImporter::extractTable(const QString &html, HtmlTable *out, QString *errorOut)
{
    const auto fail = [&](const QString &message) {
        if (errorOut)
            *errorOut = message;
        return false;
    };

    const int tableStart = findTag(html, QStringLiteral("table"), 0);
    if (tableStart < 0)
        return fail(QStringLiteral("No <table> element found in file."));
    int tableEnd = html.indexOf(QLatin1String("</table"), tableStart, Qt::CaseInsensitive);
    if (tableEnd < 0)
        tableEnd = html.size();

    out->headers.clear();
    out->rows.clear();
    out->malformedRows = 0;

    int pos = tableStart;
    bool haveHeader = false;
    while (true) {
        const int rowStart = findTag(html, QStringLiteral("tr"), pos);
        if (rowStart < 0 || rowStart >= tableEnd)
            break;
        int rowEnd = html.indexOf(QLatin1String("</tr"), rowStart, Qt::CaseInsensitive);
        if (rowEnd < 0 || rowEnd > tableEnd)
            rowEnd = tableEnd;

        QStringList cells;
        int cellPos = rowStart;
        bool headerRow = false;
        while (true) {
            const int th = findTag(html, QStringLiteral("th"), cellPos);
            const int td = findTag(html, QStringLiteral("td"), cellPos);
            int cellStart = -1;
            bool isTh = false;
            if (th >= 0 && th < rowEnd && (td < 0 || th < td)) {
                cellStart = th;
                isTh = true;
            } else if (td >= 0 && td < rowEnd) {
                cellStart = td;
            }
            if (cellStart < 0)
                break;

            const int contentStart = html.indexOf(QLatin1Char('>'), cellStart);
            if (contentStart < 0 || contentStart >= rowEnd)
                break;
            const QString closeTag = isTh ? QStringLiteral("</th") : QStringLiteral("</td");
            int contentEnd = html.indexOf(closeTag, contentStart, Qt::CaseInsensitive);
            if (contentEnd < 0 || contentEnd > rowEnd) {
                // Unclosed cell: ends at the next cell or the row end.
                const int nextTh = findTag(html, QStringLiteral("th"), contentStart + 1);
                const int nextTd = findTag(html, QStringLiteral("td"), contentStart + 1);
                contentEnd = rowEnd;
                if (nextTh >= 0 && nextTh < contentEnd)
                    contentEnd = nextTh;
                if (nextTd >= 0 && nextTd < contentEnd)
                    contentEnd = nextTd;
            }
            cells << cellText(html, contentStart + 1, contentEnd);
            if (isTh)
                headerRow = true;
            cellPos = contentEnd;
        }

        if (!haveHeader) {
            // The first row must be the header row (legacy takes trs[0] <th>s).
            if (!headerRow || cells.isEmpty())
                return fail(QStringLiteral("No <th> elements found in header row."));
            out->headers = cells;
            haveHeader = true;
        } else if (!cells.isEmpty()) {
            if (cells.size() == out->headers.size())
                out->rows.append(cells);
            else
                ++out->malformedRows;
        }

        pos = rowEnd + 1;
    }

    if (!haveHeader)
        return fail(QStringLiteral("No header row found in table."));
    return true;
}

void HtmlImporter::applyColumn(Player &player, const QString &fullColumnName, const QString &value)
{
    if (fullColumnName == QLatin1String("Name")) {
        player.name = value;
    } else if (fullColumnName == QLatin1String("Age")) {
        player.age = value.toInt();
    } else if (fullColumnName == QLatin1String("Club")) {
        player.club = value;
    } else if (fullColumnName == QLatin1String("Nationality")) {
        player.nationality = value;
    } else if (fullColumnName == QLatin1String("Second Nationality")) {
        player.secondNationality = value;
    } else if (fullColumnName == QLatin1String("Position")) {
        player.positionRaw = value;
    } else if (fullColumnName == QLatin1String("Personality")) {
        player.personality = value;
    } else if (fullColumnName == QLatin1String("Media Handling")) {
        player.mediaHandling = value;
    } else if (fullColumnName == QLatin1String("Agreed Playing Time")) {
        player.agreedPlayingTime = value;
    } else if (fullColumnName == QLatin1String("Wage")) {
        player.wageRaw = value;
    } else if (fullColumnName == QLatin1String("Transfer Value")) {
        player.transferValueRaw = value;
        player.transferValue = valueToFloat(value);
    } else if (fullColumnName == QLatin1String("Average Rating")) {
        bool ok = false;
        const double v = value.toDouble(&ok);
        player.averageRating = ok ? v : 0.0;
    } else if (fullColumnName == QLatin1String("Height")) {
        player.heightRaw = value;
        player.heightCm = LegacyMigrator::parseHeightCm(value);
    } else if (fullColumnName == QLatin1String("Left Foot")) {
        player.leftFoot = value;
    } else if (fullColumnName == QLatin1String("Right Foot")) {
        player.rightFoot = value;
    } else if (fullColumnName == QLatin1String("Preferred Foot")) {
        player.preferredFoot = value;
    } else {
        const int attrIndex = attrIndexByName(fullColumnName);
        if (attrIndex >= 0) {
            int lo = 0, hi = 0;
            LegacyMigrator::parseAttrValue(value, &lo, &hi);
            player.attrLo[attrIndex] = static_cast<uint8_t>(lo);
            player.attrHi[attrIndex] = static_cast<uint8_t>(hi);
        }
        // "Registration"/"Information" and unknown columns are ignored.
    }
}

ImportResult HtmlImporter::importHtml(const QString &html, Database &db,
                                      const std::vector<Player> &existingPlayers,
                                      std::function<void(int, int)> progress,
                                      const QString &fmVersionId)
{
    ImportResult result;
    const QHash<QString, QString> &mapping = attributeMapping(fmVersionId);

    HtmlTable table;
    QString parseError;
    if (!extractTable(html, &table, &parseError)) {
        result.error = parseError;
        return result;
    }
    if (table.headers.size() < 10) {
        result.error = QStringLiteral("Too few columns (%1), expected at least 10.")
                           .arg(table.headers.size());
        return result;
    }

    // --- Deduplicate headers: only the FIRST occurrence of a name is used
    // (legacy renames later duplicates and drops them). ---
    QSet<QString> seenHeaders;
    std::vector<bool> useColumn(static_cast<size_t>(table.headers.size()), true);
    int uidCol = -1, nameCol = -1;
    for (int i = 0; i < table.headers.size(); ++i) {
        const QString &header = table.headers.at(i);
        if (seenHeaders.contains(header)) {
            useColumn[static_cast<size_t>(i)] = false;
            continue;
        }
        seenHeaders.insert(header);
        if (header == QLatin1String("UID"))
            uidCol = i;
        else if (header == QLatin1String("Name"))
            nameCol = i;
    }
    if (uidCol < 0) {
        result.error = QStringLiteral("Required column 'UID' not found.");
        return result;
    }
    if (nameCol < 0) {
        result.error = QStringLiteral("Required column 'Name' not found.");
        return result;
    }

    // Unknown-column warning (legacy: not in attribute_mapping ∪ {UID} ∪ ignored).
    for (const QString &header : table.headers) {
        if (!mapping.contains(header) && header != QLatin1String("UID")
            && !ignoredExportColumns().contains(header)) {
            if (!result.unknownColumns.contains(header))
                result.unknownColumns << header;
        }
    }

    result.malformedRows = table.malformedRows;

    // --- UID sanity: drop empty UIDs, deduplicate (keep LAST occurrence). ---
    struct RowRef {
        int rowIndex;
        QString uid; // trimmed; corrected in the unification step
    };
    QHash<QString, int> rowByUid; // uid -> index into rowRefs
    std::vector<RowRef> rowRefs;
    rowRefs.reserve(static_cast<size_t>(table.rows.size()));

    for (int r = 0; r < table.rows.size(); ++r) {
        const QString uid = table.rows.at(r).at(uidCol).trimmed();
        if (uid.isEmpty()) {
            ++result.emptyUidRows;
            continue;
        }
        const auto it = rowByUid.constFind(uid);
        if (it != rowByUid.constEnd()) {
            const QString dupName = table.rows.at(rowRefs[it.value()].rowIndex).at(nameCol).trimmed();
            if (!result.duplicateUidNames.contains(dupName))
                result.duplicateUidNames << dupName;
            rowRefs[it.value()].rowIndex = r; // keep the last occurrence
            continue;
        }
        rowByUid.insert(uid, static_cast<int>(rowRefs.size()));
        rowRefs.push_back({r, uid});
    }

    if (rowRefs.empty()) {
        result.error = QStringLiteral("No importable rows found (all rows were missing a UID).");
        return result;
    }
    result.rowsParsed = static_cast<int>(rowRefs.size());

    // --- Lookup maps over the existing database state. ---
    QHash<QString, int> existingByUid; // uid -> index into existingPlayers
    QHash<QString, QString> numericIdToName, rIdToName;
    for (size_t i = 0; i < existingPlayers.size(); ++i) {
        const Player &p = existingPlayers[i];
        existingByUid.insert(p.uid, static_cast<int>(i));
        if (isNewgenUid(p.uid))
            rIdToName.insert(p.uid, p.name);
        else
            numericIdToName.insert(p.uid, p.name);
    }

    // Fill-empty merge of app-managed fields (legacy merge_player_records).
    const auto mergeAppManaged = [](Player &good, const Player &bad) {
        if (good.assignedRoles.isEmpty())
            good.assignedRoles = bad.assignedRoles;
        if (good.primaryRole.isEmpty())
            good.primaryRole = bad.primaryRole;
        if (good.naturalPositions.isEmpty())
            good.naturalPositions = bad.naturalPositions;
        if (good.preferredSide.isEmpty())
            good.preferredSide = bad.preferredSide;
        if (good.agreedPlayingTime.isEmpty())
            good.agreedPlayingTime = bad.agreedPlayingTime;
    };

    // uid -> merged-away duplicate whose app-managed data must be folded in.
    QHash<QString, Player> mergedSources;

    // --- ID-unification engine (legacy scenarios 1 + 2). ---
    for (RowRef &ref : rowRefs) {
        const QString incomingName = table.rows.at(ref.rowIndex).at(nameCol).trimmed();

        // Identity change: known UID arrives with a different name.
        const auto storedName = isNewgenUid(ref.uid) ? rIdToName.constFind(ref.uid)
                                                     : numericIdToName.constFind(ref.uid);
        const auto &nameMap = isNewgenUid(ref.uid) ? rIdToName : numericIdToName;
        if (storedName != nameMap.constEnd() && storedName.value() != incomingName) {
            result.identityChanges << QStringLiteral("%1: '%2' → '%3'")
                                          .arg(ref.uid, storedName.value(), incomingName);
        }

        if (!isNewgenUid(ref.uid)) {
            // Scenario 1: numeric UID exported for a known newgen (missing
            // "r-" prefix). Only numeric X -> existing r-X is a valid fix.
            const QString candidate = QStringLiteral("r-") + ref.uid;
            const auto rIt = rIdToName.constFind(candidate);
            if (rIt != rIdToName.constEnd()) {
                if (rIt.value() == incomingName) {
                    const QString numericUid = ref.uid;
                    ref.uid = candidate;
                    // A stale record under the bare numeric ID with the same
                    // name is merged away for good.
                    if (numericIdToName.value(numericUid) == incomingName) {
                        const int badIdx = existingByUid.value(numericUid, -1);
                        const int goodIdx = existingByUid.value(candidate, -1);
                        if (badIdx >= 0 && goodIdx >= 0) {
                            if (!db.mergePlayerInto(existingPlayers[badIdx].id,
                                                    existingPlayers[goodIdx].id)) {
                                result.error = db.errorString();
                                return result;
                            }
                            mergedSources.insert(candidate, existingPlayers[badIdx]);
                            existingByUid.remove(numericUid);
                            numericIdToName.remove(numericUid);
                        }
                    }
                } else {
                    result.idNameConflicts << QStringLiteral("%1 (UID %2) vs. %3 (%4)")
                                                  .arg(incomingName, ref.uid, rIt.value(),
                                                       candidate);
                }
            }
        } else {
            // Scenario 2: the file has the correct r-ID but the database
            // (also) holds a corrupted numeric record with the same name.
            const QString numericPart = ref.uid.mid(2);
            if (numericIdToName.value(numericPart) == incomingName
                && !numericPart.isEmpty()) {
                const int badIdx = existingByUid.value(numericPart, -1);
                if (badIdx >= 0) {
                    const int goodIdx = existingByUid.value(ref.uid, -1);
                    if (goodIdx >= 0) {
                        if (!db.mergePlayerInto(existingPlayers[badIdx].id,
                                                existingPlayers[goodIdx].id)) {
                            result.error = db.errorString();
                            return result;
                        }
                        mergedSources.insert(ref.uid, existingPlayers[badIdx]);
                    } else {
                        // No r-record yet: rename keeps history and every
                        // app-managed column intact.
                        if (!db.renamePlayerUid(existingPlayers[badIdx].id, ref.uid)) {
                            result.error = db.errorString();
                            return result;
                        }
                        existingByUid.insert(ref.uid, badIdx);
                        rIdToName.insert(ref.uid, incomingName);
                    }
                    existingByUid.remove(numericPart);
                    numericIdToName.remove(numericPart);
                }
            }
        }
    }

    // --- Build and upsert the player batch. ---
    // Column -> full name mapping resolved once.
    QList<QPair<int, QString>> columnTargets; // (column index, full name)
    for (int c = 0; c < table.headers.size(); ++c) {
        if (!useColumn[static_cast<size_t>(c)] || c == uidCol)
            continue;
        const auto it = mapping.constFind(table.headers.at(c));
        if (it != mapping.constEnd())
            columnTargets.append({c, it.value()});
    }

    const int total = static_cast<int>(rowRefs.size());
    int done = 0;
    std::vector<Player> batch;
    batch.reserve(kUpsertBatchSize);

    const auto flush = [&]() -> bool {
        if (batch.empty())
            return true;
        if (!db.upsertPlayers(batch)) {
            result.error = db.errorString();
            return false;
        }
        result.playersImported += static_cast<int>(batch.size());
        batch.clear();
        if (progress)
            progress(done, total);
        return true;
    };

    for (const RowRef &ref : rowRefs) {
        const QStringList &cells = table.rows.at(ref.rowIndex);

        Player player;
        const int existingIdx = existingByUid.value(ref.uid, -1);
        if (existingIdx >= 0) {
            player = existingPlayers[static_cast<size_t>(existingIdx)];
            player.uid = ref.uid; // rename case: keep id, adopt the r-uid
        } else {
            ++result.newPlayers;
        }
        const auto mergedIt = mergedSources.constFind(ref.uid);
        if (mergedIt != mergedSources.constEnd())
            mergeAppManaged(player, mergedIt.value());

        player.uid = ref.uid;
        for (const auto &[column, fullName] : columnTargets)
            applyColumn(player, fullName, cells.at(column).trimmed());
        if (player.name.isEmpty())
            player.name = cells.at(nameCol).trimmed();

        result.affectedUids << ref.uid;
        batch.push_back(std::move(player));
        ++done;
        if (static_cast<int>(batch.size()) >= kUpsertBatchSize && !flush())
            return result;
    }
    if (!flush())
        return result;

    result.success = true;
    return result;
}

ImportResult HtmlImporter::importFile(const QString &filePath, Database &db,
                                      const std::vector<Player> &existingPlayers,
                                      std::function<void(int, int)> progress,
                                      const QString &fmVersionId)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ImportResult result;
        result.error = QStringLiteral("Could not open file: %1").arg(file.errorString());
        return result;
    }
    const QString html = QString::fromUtf8(file.readAll());
    return importHtml(html, db, existingPlayers, std::move(progress), fmVersionId);
}

QString HtmlImporter::forceUpdateSinglePlayer(const QString &html, Database &db,
                                              const std::vector<Player> &existingPlayers,
                                              const QString &targetUid, QString *errorOut,
                                              const QString &fmVersionId)
{
    const QHash<QString, QString> &mapping = attributeMapping(fmVersionId);
    const auto fail = [&](const QString &message) {
        if (errorOut)
            *errorOut = message;
        return QString();
    };

    HtmlTable table;
    QString parseError;
    if (!extractTable(html, &table, &parseError))
        return fail(parseError);
    if (table.rows.size() != 1)
        return fail(QStringLiteral("The file must contain exactly ONE player, "
                                   "but %1 rows were found.")
                        .arg(table.rows.size()));

    const Player *existing = nullptr;
    for (const Player &p : existingPlayers) {
        if (p.uid == targetUid) {
            existing = &p;
            break;
        }
    }
    if (!existing)
        return fail(QStringLiteral("Target player %1 not found.").arg(targetUid));

    Player player = *existing; // app-managed fields preserved
    QSet<QString> seenHeaders;
    QString fileName;
    for (int c = 0; c < table.headers.size(); ++c) {
        const QString &header = table.headers.at(c);
        if (seenHeaders.contains(header))
            continue;
        seenHeaders.insert(header);
        const auto it = mapping.constFind(header);
        if (it == mapping.constEnd())
            continue;
        const QString value = table.rows.at(0).at(c).trimmed();
        if (it.value() == QLatin1String("Name"))
            fileName = value;
        applyColumn(player, it.value(), value);
    }
    player.uid = targetUid; // everything is keyed to the confirmed target

    std::vector<Player> batch{std::move(player)};
    if (!db.upsertPlayers(batch))
        return fail(db.errorString());
    return fileName;
}

} // namespace fm
