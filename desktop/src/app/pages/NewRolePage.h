#pragma once

#include "PageBase.h"

#include <QHash>
#include <QPair>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace fm {

// Editor for new field-player roles: name/category/duty, position mapping and
// key/preferable attributes; writes into definitions.json.
// Port of legacy new_role.py.
class NewRolePage : public PageBase
{
    Q_OBJECT

public:
    explicit NewRolePage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    void updateShortName();
    QString shortName() const;
    void createRole();
    void resetForm();

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_categoryCombo = nullptr;
    QComboBox *m_dutyCombo = nullptr;
    QLabel *m_shortNameLabel = nullptr;
    QListWidget *m_positionList = nullptr;
    // attribute -> (key checkbox, preferable checkbox)
    QHash<QString, QPair<QCheckBox *, QCheckBox *>> m_attrChecks;
    QPushButton *m_createButton = nullptr;
};

} // namespace fm
