#pragma once

#include "PageBase.h"

#include <QStringList>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QTableView;

namespace fm {

struct Player;
class PersonalityFilterWidget;
class PlayerTableModel;
class PlayerFilterProxy;
struct PlayerColumn;

// DWRS matrix for the national squad and the eligible player pool (no
// financial columns). Port of legacy national_squad_matrix.py.
class NationalSquadMatrixPage : public PageBase
{
    Q_OBJECT

public:
    explicit NationalSquadMatrixPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;
    void releaseStoreRows() override;

private:
    struct Section {
        QGroupBox *box = nullptr;
        QLineEdit *search = nullptr;
        PlayerTableModel *model = nullptr;
        PlayerFilterProxy *proxy = nullptr;
        QTableView *table = nullptr;
    };

    void buildSection(Section *section, const QString &title, QWidget *parent);
    QList<PlayerColumn> buildColumns() const;
    QStringList selectedRoles() const;
    void rebuild();

    QComboBox *m_tacticCombo = nullptr;
    QCheckBox *m_extraDetailsCheck = nullptr;
    QCheckBox *m_hideRetiredCheck = nullptr;
    PersonalityFilterWidget *m_personalityFilter = nullptr;

    Section m_squadSection;
    Section m_poolSection;
    bool m_updating = false;
};

} // namespace fm
