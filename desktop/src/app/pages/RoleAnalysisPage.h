#pragma once

#include "PageBase.h"

class QButtonGroup;
class QComboBox;
class QLabel;
class QTabWidget;
class QTableView;

namespace fm {

class PersonalityFilterWidget;
class PlayerTableModel;
class PlayerFilterProxy;

// Players per role (club / second team / scouted) plus the strengths &
// weaknesses report. Port of legacy role_analysis.py.
class RoleAnalysisPage : public PageBase
{
    Q_OBJECT

public:
    explicit RoleAnalysisPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    struct Group {
        PlayerTableModel *model = nullptr;
        PlayerFilterProxy *proxy = nullptr;
        QTableView *table = nullptr;
        int tabIndex = -1;
    };

    QWidget *buildGroupTab(Group *group);
    void rebuildTables();
    void rebuildProsCons();
    void rebuildPlayerCombo();
    QString selectedRole() const;

    QComboBox *m_roleCombo = nullptr;
    PersonalityFilterWidget *m_personalityFilter = nullptr;
    QTabWidget *m_tabs = nullptr;
    Group m_myClub, m_secondTeam, m_scouted;

    QLabel *m_prosConsTitle = nullptr;
    QButtonGroup *m_scopeGroup = nullptr;
    QWidget *m_scopeRow = nullptr;
    QComboBox *m_playerCombo = nullptr;
    QLabel *m_prosLabel = nullptr;
    QLabel *m_consLabel = nullptr;

    bool m_updating = false;
};

} // namespace fm
