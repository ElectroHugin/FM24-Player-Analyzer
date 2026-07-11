#pragma once

#include "PageBase.h"

#include <vector>

class QComboBox;
class QGroupBox;
class QLabel;
class QTableWidget;
class QTabWidget;

namespace fm {

struct Player;
class TacticPitchWidget;
class ThemeManager;
struct SquadResult;
struct DevelopmentSquads;

// Best XI calculator: starting XI, B-team, depth options plus the youth /
// second-team development squads and surplus lists.
// Port of legacy best_position.py.
class BestXiPage : public PageBase
{
    Q_OBJECT

public:
    BestXiPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuild();
    void fillSurplusTable(QTableWidget *table, const std::vector<const Player *> &players,
                          bool includeTalent);

    ThemeManager &m_theme;

    QComboBox *m_tacticCombo = nullptr;
    QLabel *m_hint = nullptr;
    QTabWidget *m_tabs = nullptr;

    TacticPitchWidget *m_xiPitch = nullptr;
    TacticPitchWidget *m_bTeamPitch = nullptr;
    QLabel *m_depthLabel = nullptr;

    QLabel *m_secondXiTitle = nullptr;
    TacticPitchWidget *m_secondXiPitch = nullptr;
    TacticPitchWidget *m_youthXiPitch = nullptr;
    QGroupBox *m_loanBox = nullptr;
    QTableWidget *m_loanTable = nullptr;
    QGroupBox *m_sellBox = nullptr;
    QTableWidget *m_sellTable = nullptr;

    bool m_updating = false;
};

} // namespace fm
