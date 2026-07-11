#pragma once

#include "PageBase.h"

#include <QStringList>

#include <vector>

class QComboBox;
class QGroupBox;
class QLabel;
class QTableWidget;

namespace fm {

struct Player;

// Definitive surplus lists (loan candidates and sell/release candidates) for
// the selected tactic, with transfer/loan status and new-club editing.
// Port of legacy transfer_loan_management.py.
class TransfersPage : public PageBase
{
    Q_OBJECT

public:
    explicit TransfersPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    struct Section {
        QGroupBox *box = nullptr;
        QTableWidget *table = nullptr;
        QString baseTitle;
        bool youth = false;
    };

    void buildSectionUi(Section *section, const QString &title, bool youth, QWidget *parent);
    void fillTable(Section &section, const std::vector<const Player *> &players);
    void saveSection(const Section &section);
    void rebuild();

    QComboBox *m_tacticCombo = nullptr;
    QLabel *m_hint = nullptr;
    Section m_loanSection;
    Section m_sellSection;
    bool m_updating = false;
};

} // namespace fm
