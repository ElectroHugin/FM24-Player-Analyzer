#pragma once

#include "PageBase.h"

#include <QHash>
#include <QStringList>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QTableView;

namespace fm {

struct Player;
class PersonalityFilterWidget;
class PlayerTableModel;
class PlayerFilterProxy;
struct PlayerColumn;

// DWRS matrix (players x roles) for club, second team and scouted players,
// with talent filter, personality filter, shortlist and CSV export.
// Port of legacy player_role_matrix.py.
class SquadMatrixPage : public PageBase
{
    Q_OBJECT

public:
    explicit SquadMatrixPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    struct TableSection {
        QGroupBox *box = nullptr;
        QLineEdit *search = nullptr;
        PlayerTableModel *model = nullptr;
        PlayerFilterProxy *proxy = nullptr;
        QTableView *table = nullptr;
    };

    void buildSection(TableSection *section, const QString &title, QWidget *parent,
                      bool scouted);
    QList<PlayerColumn> buildColumns(bool clubTable, bool withShortlist) const;
    void rebuildAll();
    void rebuildScoutedOnly();
    QStringList selectedRoles() const;
    void toggleShortlist(const Player *player);
    void exportCsv(const TableSection &section);

    // Options
    QComboBox *m_tacticCombo = nullptr;
    QCheckBox *m_extraDetailsCheck = nullptr;
    QCheckBox *m_secondTeamCheck = nullptr;
    QCheckBox *m_hideRetiredCheck = nullptr;
    PersonalityFilterWidget *m_personalityFilter = nullptr;

    // Talent filter
    QGroupBox *m_talentBox = nullptr;
    QButtonGroup *m_talentScope = nullptr;
    QSlider *m_talentAgeSlider = nullptr;
    QLabel *m_talentAgeLabel = nullptr;
    QSlider *m_talentMentalitySlider = nullptr;
    QLabel *m_talentMentalityLabel = nullptr;
    QCheckBox *m_talentGoodOnlyCheck = nullptr;

    // Scouted advanced filter
    QComboBox *m_scoutedFilterCombo = nullptr;
    QSpinBox *m_dwrsMinSpin = nullptr;
    QSpinBox *m_dwrsMaxSpin = nullptr;
    QSlider *m_maxAgeSlider = nullptr;
    QLabel *m_maxAgeLabel = nullptr;
    QSlider *m_maxValueSlider = nullptr;
    QLabel *m_maxValueLabel = nullptr;

    TableSection m_clubSection;
    TableSection m_secondSection;
    TableSection m_scoutedSection;

    std::vector<const Player *> m_scoutedPool; // after shared filters
    QHash<QString, double> m_talentScores;     // uid -> score (talent filter on)
    bool m_updating = false;
};

} // namespace fm
