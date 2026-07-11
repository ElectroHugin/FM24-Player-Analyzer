#pragma once

#include "PageBase.h"

#include "core/HtmlImporter.h"
#include "core/RatingsUpdater.h"

#include <QFutureWatcher>
#include <QSet>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressDialog;
class QPushButton;
class QSlider;
class QTableWidget;
class QTimer;

namespace fm {

class StrengthGridWidget;
class ThemeManager;

// Everything the background import pipeline produces (import + optional
// auto-assign + DWRS recalculation for the affected players).
struct ImportPipelineResult {
    ImportResult import;
    QString backupError;       // non-fatal
    QStringList autoAssignedUids;
    QString autoAssignError;   // non-fatal
    bool recalcRan = false;
    RatingsUpdater::Result recalc;
};

// Club-mode dashboard: HTML import, tactic-based squad KPIs, positional
// strength grid, club roster and transfer-target suggestions.
// Port of legacy app.main_page.
class DashboardPage : public PageBase
{
    Q_OBJECT

public:
    DashboardPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    QWidget *buildImportSection(QWidget *parent);
    QWidget *buildAnalysisHeader(QWidget *parent);
    QWidget *buildKpiRow(QWidget *parent);
    QWidget *buildSquadSection(QWidget *parent);
    QWidget *buildSuggestionSection(QWidget *parent);

    void chooseImportFile();
    void startImport();
    void importFinished();
    void resolveDepartures(const QSet<QString> &affectedUids);

    void refreshCombos();
    void refreshAnalysis();
    void updateKpis(const std::vector<const Player *> &coreSquad);
    void updateClubTable(const std::vector<const Player *> &clubPlayers);
    void rebuildSuggestions();
    void clearSuggestionCards();

    QString selectedTactic() const;
    double maxValueFilter() const;

    ThemeManager &m_theme;

    // Import section
    QLineEdit *m_filePathEdit = nullptr;
    QCheckBox *m_squadUpdateCheck = nullptr;
    QCheckBox *m_autoAssignCheck = nullptr;
    QPushButton *m_importButton = nullptr;
    QFutureWatcher<ImportPipelineResult> m_importWatcher;
    QProgressDialog *m_importDialog = nullptr;
    bool m_pendingSquadUpdate = false;

    // Analysis header
    QComboBox *m_clubCombo = nullptr;
    QComboBox *m_tacticCombo = nullptr;
    QLabel *m_analysisHint = nullptr;

    // KPI tiles
    QLabel *m_kpiPlayers = nullptr;
    QLabel *m_kpiTotalValue = nullptr;
    QLabel *m_kpiAvgValue = nullptr;
    QLabel *m_kpiAvgAge = nullptr;
    QWidget *m_kpiRow = nullptr;

    // Squad section
    QWidget *m_squadSection = nullptr;
    StrengthGridWidget *m_strengthGrid = nullptr;
    QLabel *m_clubTableTitle = nullptr;
    QTableWidget *m_clubTable = nullptr;

    // Transfer suggestions
    QGroupBox *m_suggestionBox = nullptr;
    QSlider *m_ageSlider = nullptr;
    QLabel *m_ageLabel = nullptr;
    QSlider *m_valueSlider = nullptr;
    QLabel *m_valueLabel = nullptr;
    QLabel *m_suggestionStatus = nullptr;
    QGridLayout *m_cardsLayout = nullptr;
    QTimer *m_suggestionDebounce = nullptr;

    bool m_updatingCombos = false;
};

} // namespace fm
