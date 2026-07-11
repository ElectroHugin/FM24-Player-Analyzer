#pragma once

#include "core/HtmlImporter.h"
#include "core/RatingsUpdater.h"

#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

namespace fm {

class AppContext;

// Everything the background import pipeline produces (import + optional
// auto-assign + DWRS recalculation for the affected players).
struct ImportPipelineResult {
    ImportResult import;
    QString backupError;     // non-fatal
    QStringList autoAssignedUids;
    QString autoAssignError; // non-fatal
    bool recalcRan = false;
    RatingsUpdater::Result recalc;
};

// Runs the full HTML-import pipeline (backup, parse+import, optional
// auto-assign, DWRS recalc for affected players) in a background thread with
// a modal progress dialog. On finish the context is reloaded when the import
// succeeded, then onDone(result) runs on the UI thread.
// Shared by the club dashboard and the national dashboard.
void runImportPipeline(AppContext &context, QWidget *parent, const QString &filePath,
                       bool autoAssign, std::function<void(ImportPipelineResult)> onDone);

// Builds the localized summary + warning lists for a finished pipeline run.
QStringList importSummaryLines(const ImportPipelineResult &result);
QStringList importWarningLines(const ImportPipelineResult &result);

} // namespace fm
