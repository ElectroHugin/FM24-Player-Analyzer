#pragma once

#include "PageBase.h"

#include <QStringList>

class QComboBox;
class QLabel;
class QListWidget;
class QStackedWidget;

namespace fm {

struct Player;
class LineChartWidget;
class ThemeManager;

// DWRS development trends in three modes: squad averages by role, player vs
// player in one role, and an individual deep dive.
// Port of legacy dwrs_progress.py.
class DwrsProgressPage : public PageBase
{
    Q_OBJECT

public:
    DwrsProgressPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuildFilters();
    void rebuildChart();
    QStringList checkedItems(QListWidget *list) const;
    std::vector<const Player *> clubPlayers() const;
    void fillTacticCombo(QComboBox *combo);
    QStringList rolesForTactic(const QString &tactic) const;

    ThemeManager &m_theme;

    QComboBox *m_modeCombo = nullptr;
    QStackedWidget *m_filterStack = nullptr;

    // Mode 1: squad overview
    QComboBox *m_squadTacticCombo = nullptr;
    QListWidget *m_squadRoleList = nullptr;

    // Mode 2: player vs player
    QComboBox *m_pvpTacticCombo = nullptr;
    QComboBox *m_pvpRoleCombo = nullptr;
    QListWidget *m_pvpPlayerList = nullptr;

    // Mode 3: individual deep dive
    QComboBox *m_individualPlayerCombo = nullptr;
    QListWidget *m_individualRoleList = nullptr;

    QLabel *m_chartTitle = nullptr;
    LineChartWidget *m_chart = nullptr;

    bool m_updating = false;
};

} // namespace fm
