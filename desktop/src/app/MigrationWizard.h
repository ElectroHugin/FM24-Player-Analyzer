#pragma once

#include "core/LegacyMigrator.h"

#include <QDialog>
#include <QFutureWatcher>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QPlainTextEdit;

namespace fm {

class AppContext;

// Imports a legacy Streamlit-era database (plus optionally its config.ini,
// definitions.json, logo and flag) into the new data directory. Runs the
// migration on a worker thread with live progress.
class MigrationWizard : public QDialog
{
    Q_OBJECT

public:
    explicit MigrationWizard(AppContext &context, QWidget *parent = nullptr);

    // Name of the migrated database if the user completed a migration.
    QString migratedDbName() const { return m_migratedDbName; }

private:
    void browseSource();
    void startMigration();
    void migrationFinished();

    AppContext &m_context;
    QLineEdit *m_sourceEdit;
    QLineEdit *m_targetNameEdit;
    QCheckBox *m_importExtras;
    QProgressBar *m_progress;
    QPlainTextEdit *m_log;
    QPushButton *m_startButton;
    QPushButton *m_closeButton;
    QFutureWatcher<MigrationStats> m_watcher;
    QString m_migratedDbName;
};

} // namespace fm
