#pragma once

#include "PageBase.h"

#include <vector>

class QComboBox;
class QLabel;
class QTableWidget;
class QTabWidget;

namespace fm {

struct Gap;
class TacticPitchWidget;
class ThemeManager;

// Squad gap analysis: obvious drop-off gaps and hidden displacement gaps in
// the starting XI and the B-team, as table and heat pitch.
// Port of legacy gap_analysis.py.
class GapAnalysisPage : public PageBase
{
    Q_OBJECT

public:
    GapAnalysisPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    struct TeamView {
        QLabel *status = nullptr;
        QTableWidget *table = nullptr;
        TacticPitchWidget *pitch = nullptr;
    };

    QWidget *buildTeamTab(TeamView *view);
    void rebuild();
    void showGaps(TeamView &view, const std::vector<Gap> &gaps);

    ThemeManager &m_theme;

    QComboBox *m_tacticCombo = nullptr;
    QLabel *m_thresholdCaption = nullptr;
    QTabWidget *m_tabs = nullptr;
    TeamView m_xiView;
    TeamView m_bTeamView;

    bool m_updating = false;
};

} // namespace fm
