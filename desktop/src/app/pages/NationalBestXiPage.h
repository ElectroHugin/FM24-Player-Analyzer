#pragma once

#include "PageBase.h"

class QComboBox;
class QLabel;

namespace fm {

class TacticPitchWidget;
class ThemeManager;

// Best XI, B-team and depth options for the national squad (no APT
// weighting). Port of legacy national_best_xi.py.
class NationalBestXiPage : public PageBase
{
    Q_OBJECT

public:
    NationalBestXiPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuild();

    ThemeManager &m_theme;
    QComboBox *m_tacticCombo = nullptr;
    QLabel *m_hint = nullptr;
    QWidget *m_content = nullptr;
    TacticPitchWidget *m_xiPitch = nullptr;
    TacticPitchWidget *m_bTeamPitch = nullptr;
    QLabel *m_depthLabel = nullptr;
    bool m_updating = false;
};

} // namespace fm
