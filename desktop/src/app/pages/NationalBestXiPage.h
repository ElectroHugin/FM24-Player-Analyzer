#pragma once

#include "PageBase.h"

#include <QHash>
#include <QList>

class QComboBox;
class QGroupBox;
class QLabel;
class QTableWidget;

namespace fm {

class TacticPitchWidget;
class ThemeManager;
struct DepthOption;

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
    void fillDepthTable(const QHash<QString, QList<DepthOption>> &options);

    ThemeManager &m_theme;
    QComboBox *m_tacticCombo = nullptr;
    QLabel *m_hint = nullptr;
    QWidget *m_content = nullptr;
    TacticPitchWidget *m_xiPitch = nullptr;
    TacticPitchWidget *m_bTeamPitch = nullptr;
    QGroupBox *m_depthBox = nullptr;
    QTableWidget *m_depthTable = nullptr;
    QLabel *m_depthEmpty = nullptr;
    bool m_updating = false;
};

} // namespace fm
