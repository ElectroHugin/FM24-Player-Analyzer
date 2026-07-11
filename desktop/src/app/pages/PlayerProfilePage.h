#pragma once

#include "PageBase.h"

class QButtonGroup;
class QComboBox;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QWidget;

namespace fm {

struct Player;
class LineChartWidget;
class ThemeManager;

// Player profile: vitals, status tags, top roles, talent projection,
// strengths/weaknesses for the best role, key attributes and the DWRS
// development chart. Port of legacy player_profile.py.
class PlayerProfilePage : public PageBase
{
    Q_OBJECT

public:
    PlayerProfilePage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuildPlayerCombo();
    void showPlayer();
    void runManualUpdate();

    ThemeManager &m_theme;

    QButtonGroup *m_scopeGroup = nullptr;
    QComboBox *m_playerCombo = nullptr;

    QLabel *m_nameLabel = nullptr;
    QLabel *m_vitalsLabel = nullptr;
    QLabel *m_tagsLabel = nullptr;

    QGroupBox *m_updateBox = nullptr;
    QLineEdit *m_updateFileEdit = nullptr;
    QCheckBox *m_updateConfirm = nullptr;
    QPushButton *m_updateButton = nullptr;

    QWidget *m_detailWidget = nullptr;
    QHBoxLayout *m_topRolesLayout = nullptr;
    QGroupBox *m_talentBox = nullptr;
    QLabel *m_talentLabel = nullptr;
    QLabel *m_analysisTitle = nullptr;
    QLabel *m_prosLabel = nullptr;
    QLabel *m_consLabel = nullptr;
    QLabel *m_keyAttrsLabel = nullptr;
    LineChartWidget *m_chart = nullptr;
    QLabel *m_noRolesLabel = nullptr;

    bool m_updating = false;
};

} // namespace fm
