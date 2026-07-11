#pragma once

#include "PageBase.h"

#include <QSet>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace fm {

// Build the national squad from the eligible player pool (nationality +
// age limit). Port of legacy national_squad_selection.py.
class NationalSquadSelectionPage : public PageBase
{
    Q_OBJECT

public:
    explicit NationalSquadSelectionPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuildLists();
    void moveSelected(QListWidget *from, bool adding);
    void save();

    QLabel *m_hint = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QListWidget *m_availableList = nullptr;
    QLabel *m_squadTitle = nullptr;
    QListWidget *m_squadList = nullptr;
    QPushButton *m_saveButton = nullptr;

    QSet<QString> m_selection;  // uids currently chosen (unsaved)
    bool m_dirty = false;
};

} // namespace fm
