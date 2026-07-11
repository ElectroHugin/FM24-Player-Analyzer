#pragma once

#include "PageBase.h"

#include <QHash>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace fm {

// Editor for new tactical formations: pick a role per pitch slot (exactly one
// goalkeeper and ten outfield players); writes tactic_roles + tactic_layouts
// into definitions.json. Port of legacy new_tactic.py.
class NewTacticPage : public PageBase
{
    Q_OBJECT

public:
    explicit NewTacticPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    void updateFullName();
    QString fullName() const;
    void createTactic();
    void resetForm();
    void fillSlotCombo(QComboBox *combo, const QStringList &gamePositions);

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_shapeEdit = nullptr;
    QLabel *m_fullNameLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QHash<QString, QComboBox *> m_slotCombos; // slot key -> role combo (incl. GK)
    QPushButton *m_createButton = nullptr;
};

} // namespace fm
