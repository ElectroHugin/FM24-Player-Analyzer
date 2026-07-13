#include "ImportRunner.h"

#include "AppContext.h"
#include "core/Database.h"
#include "core/RoleAssignment.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QPointer>
#include <QProgressDialog>
#include <QSet>
#include <QtConcurrentRun>

namespace fm {

namespace {

QString trRunner(const char *text)
{
    return QCoreApplication::translate("ImportRunner", text);
}

} // namespace

void runImportPipeline(AppContext &context, QWidget *parent, const QString &filePath,
                       bool autoAssign, std::function<void(ImportPipelineResult)> onDone)
{
    auto *dialog = new QProgressDialog(trRunner("Import wird vorbereitet…"), QString(), 0, 100,
                                       parent);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setMinimumWidth(420);
    dialog->setValue(0);

    // Snapshot everything the worker needs; it opens its own DB connection.
    const QString dbFile = context.database().filePath();
    const QString backupsDir = context.paths().backupsDir();
    const QStringList validRoles = context.definitions().validRoles();
    const Definitions *definitions = &context.definitions();
    const DwrsEngine *engine = &context.dwrsEngine();
    const QString fmVersion = context.fmVersionId();

    // Guard against the dialog being destroyed (e.g. window closed) while the
    // worker thread is still posting progress. qApp is a stable context object
    // living on the GUI thread; the QPointer is re-checked there before use.
    const auto stage = [dialogGuard = QPointer<QProgressDialog>(dialog)](const QString &text,
                                                                         int percent) {
        QMetaObject::invokeMethod(
            qApp,
            [dialogGuard, text, percent] {
                if (dialogGuard) {
                    dialogGuard->setLabelText(text);
                    dialogGuard->setValue(percent);
                }
            },
            Qt::QueuedConnection);
    };

    auto *watcher = new QFutureWatcher<ImportPipelineResult>(parent);
    QObject::connect(watcher, &QFutureWatcher<ImportPipelineResult>::finished, parent,
                     [watcher, dialog, &context, onDone = std::move(onDone)] {
                         const ImportPipelineResult result = watcher->result();
                         watcher->deleteLater();
                         dialog->close();
                         dialog->deleteLater();
                         if (result.import.success)
                             context.reloadFromDatabase();
                         if (onDone)
                             onDone(result);
                     });

    watcher->setFuture(QtConcurrent::run([filePath, dbFile, backupsDir, autoAssign, validRoles,
                                          definitions, engine, fmVersion, stage] {
        ImportPipelineResult result;

        stage(trRunner("Backup wird erstellt…"), 2);
        QString backupError;
        if (!Database::createBackup(dbFile, backupsDir, &backupError))
            result.backupError = backupError;

        Database db(QStringLiteral("import_worker"));
        if (!db.open(dbFile)) {
            result.import.error = db.errorString();
            return result;
        }

        stage(trRunner("Datei wird analysiert…"), 5);
        std::vector<Player> players = db.loadPlayers();
        result.import = HtmlImporter::importFile(
            filePath, db, players,
            [&stage](int done, int total) {
                // 64-bit math: byte offsets on a large export overflow int.
                const int percent =
                    15 + (total > 0 ? static_cast<int>(qint64(done) * 40 / total) : 0);
                stage(trRunner("Spieler werden importiert… (%1/%2)")
                          .arg(done)
                          .arg(total),
                      percent);
            },
            fmVersion,
            [&stage](int done, int total) {
                const int percent =
                    5 + (total > 0 ? static_cast<int>(qint64(done) * 10 / total) : 0);
                stage(trRunner("Datei wird analysiert…"), percent);
            });
        if (!result.import.success)
            return result;

        players = db.loadPlayers();

        if (autoAssign) {
            stage(trRunner("Rollen werden automatisch zugewiesen…"), 58);
            result.autoAssignedUids = RoleAssignment::autoAssignRolesToUnassigned(
                db, players, *definitions, &result.autoAssignError);
        }

        // DWRS only for the players this import (or auto-assign) touched.
        QSet<QString> affected(result.import.affectedUids.cbegin(),
                               result.import.affectedUids.cend());
        for (const QString &uid : std::as_const(result.autoAssignedUids))
            affected.insert(uid);
        std::vector<int> subset;
        subset.reserve(affected.size());
        for (size_t i = 0; i < players.size(); ++i) {
            if (affected.contains(players[i].uid))
                subset.push_back(static_cast<int>(i));
        }

        stage(trRunner("DWRS-Bewertungen werden berechnet…"), 62);
        result.recalcRan = true;
        result.recalc = RatingsUpdater::updateDwrsRatings(
            db, players, *engine, validRoles, subset, [&stage](int current, int total) {
                const int percent = 62 + (total > 0 ? current * 38 / total : 0);
                stage(trRunner("DWRS-Bewertungen werden berechnet…"), percent);
            });
        return result;
    }));
}

QStringList importSummaryLines(const ImportPipelineResult &result)
{
    QStringList summary;
    summary << trRunner("%1 Spieler importiert (davon %2 neu).")
                   .arg(result.import.playersImported)
                   .arg(result.import.newPlayers);
    if (!result.autoAssignedUids.isEmpty())
        summary << trRunner("%1 Spielern wurden automatisch Rollen zugewiesen.")
                       .arg(result.autoAssignedUids.size());
    if (result.recalcRan && result.recalc.success)
        summary << trRunner("DWRS: %1 Bewertungen berechnet, %2 geänderte Einträge gespeichert.")
                       .arg(result.recalc.computed)
                       .arg(result.recalc.inserted);
    return summary;
}

QStringList importWarningLines(const ImportPipelineResult &result)
{
    QStringList warnings;
    if (!result.backupError.isEmpty())
        warnings << trRunner("Backup fehlgeschlagen: %1").arg(result.backupError);
    if (result.import.malformedRows > 0)
        warnings << trRunner("%1 fehlerhafte Zeile(n) übersprungen (Zellenzahl passte nicht "
                             "zur Kopfzeile). Exportiere die Datei ggf. neu aus FM.")
                        .arg(result.import.malformedRows);
    if (result.import.emptyUidRows > 0)
        warnings << trRunner("%1 Zeile(n) ohne UID übersprungen.")
                        .arg(result.import.emptyUidRows);
    if (!result.import.duplicateUidNames.isEmpty())
        warnings << trRunner("Doppelte UIDs in der Datei — nur die letzte Zeile wurde "
                             "übernommen: %1")
                        .arg(result.import.duplicateUidNames.mid(0, 10)
                                 .join(QStringLiteral(", ")));
    if (!result.import.unknownColumns.isEmpty())
        warnings << trRunner("Unbekannte Spalten (werden ignoriert): %1")
                        .arg(result.import.unknownColumns.join(QStringLiteral(", ")));
    if (!result.import.idNameConflicts.isEmpty())
        warnings << trRunner("ID/Namens-Konflikt übersprungen: %1. Eine numerische UID "
                             "entspricht einer bekannten Newgen-ID, aber die Namen "
                             "unterscheiden sich.")
                        .arg(result.import.idNameConflicts.mid(0, 10)
                                 .join(QStringLiteral("; ")));
    if (!result.import.identityChanges.isEmpty())
        warnings << trRunner("Name unter bekannter UID geändert: %1. FM hat die ID evtl. an "
                             "einen neuen Newgen vergeben — zugewiesene Rollen und "
                             "DWRS-Historie gehören ggf. noch zum alten Spieler.")
                        .arg(result.import.identityChanges.mid(0, 10)
                                 .join(QStringLiteral("; ")));
    if (!result.autoAssignError.isEmpty())
        warnings << trRunner("Automatische Rollen-Zuweisung fehlgeschlagen: %1")
                        .arg(result.autoAssignError);
    if (result.recalcRan && !result.recalc.success)
        warnings << trRunner("DWRS-Berechnung fehlgeschlagen: %1").arg(result.recalc.error);
    return warnings;
}

} // namespace fm
