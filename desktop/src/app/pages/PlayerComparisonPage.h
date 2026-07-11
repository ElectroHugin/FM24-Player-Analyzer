#pragma once

#include "PageBase.h"

#include <QStringList>

class QComboBox;
class QLabel;
class QListWidget;
class QTableWidget;

namespace fm {

struct Player;
class RadarChartWidget;
class ThemeManager;

// Compare up to five players in a role: two radar charts (gameplay areas and
// role-weighted meta attributes) plus the detailed attribute table.
// Port of legacy player_comparison.py.
class PlayerComparisonPage : public PageBase
{
    Q_OBJECT

public:
    PlayerComparisonPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuildRoleCombo();
    void rebuildPlayerList();
    void rebuildComparison();
    QStringList selectedUids() const;

    ThemeManager &m_theme;

    QComboBox *m_tacticCombo = nullptr;
    QComboBox *m_roleCombo = nullptr;
    QComboBox *m_poolCombo = nullptr;
    QListWidget *m_playerList = nullptr;
    QLabel *m_hint = nullptr;
    QStringList m_pendingCheck; // uids handed over by "Zum Vergleich hinzufügen"

    QLabel *m_gameplayTitle = nullptr;
    QLabel *m_metaTitle = nullptr;
    RadarChartWidget *m_gameplayChart = nullptr;
    RadarChartWidget *m_metaChart = nullptr;
    QTableWidget *m_detailTable = nullptr;

    bool m_updating = false;
};

} // namespace fm
