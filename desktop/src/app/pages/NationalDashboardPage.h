#pragma once

#include "PageBase.h"

#include "../ImportRunner.h"

class QCheckBox;
class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTableWidget;

namespace fm {

class StrengthGridWidget;
class ThemeManager;

// National dashboard: HTML import (optionally replacing the squad), KPIs,
// positional strength grid, squad table and potential call-ups.
// Port of legacy national_dashboard.py.
class NationalDashboardPage : public PageBase
{
    Q_OBJECT

public:
    NationalDashboardPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    void startImport();
    void importFinished(const ImportPipelineResult &result);
    void rebuildAnalysis();
    void rebuildCallUps();
    void clearCards();

    ThemeManager &m_theme;

    // Import
    QLineEdit *m_filePathEdit = nullptr;
    QCheckBox *m_replaceSquadCheck = nullptr;
    QCheckBox *m_autoAssignCheck = nullptr;
    QPushButton *m_importButton = nullptr;
    bool m_importRunning = false;

    QLabel *m_hint = nullptr;
    QWidget *m_analysisWidget = nullptr;
    QComboBox *m_tacticCombo = nullptr;
    QLabel *m_kpiPlayers = nullptr;
    QLabel *m_kpiAvgAge = nullptr;
    StrengthGridWidget *m_strengthGrid = nullptr;
    QLabel *m_squadTableTitle = nullptr;
    QTableWidget *m_squadTable = nullptr;

    QSlider *m_maxAgeSlider = nullptr;
    QLabel *m_maxAgeLabel = nullptr;
    QLabel *m_callUpStatus = nullptr;
    QGridLayout *m_cardsLayout = nullptr;

    bool m_updating = false;
};

} // namespace fm
