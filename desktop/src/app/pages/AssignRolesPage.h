#pragma once

#include "PageBase.h"

#include <QHash>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableView;

namespace fm {

struct Player;
class PlayerTableModel;
class PlayerFilterProxy;

// Assign/edit player roles: filterable table, per-player role editor and the
// two auto-assign actions. Port of legacy assign_roles.py.
class AssignRolesPage : public PageBase
{
    Q_OBJECT

public:
    explicit AssignRolesPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuildFilters();
    void applyFilters();
    void showEditorFor(const Player *player);
    void editorSelectionChanged();
    void stagePendingFromEditor();
    void savePending();
    void autoAssign(bool allPlayers);
    QStringList editorRoles() const;

    QComboBox *m_filterCombo = nullptr;
    QComboBox *m_clubCombo = nullptr;
    QComboBox *m_positionCombo = nullptr;
    QLineEdit *m_searchEdit = nullptr;

    PlayerTableModel *m_model = nullptr;
    PlayerFilterProxy *m_proxy = nullptr;
    QTableView *m_table = nullptr;

    QLabel *m_editorTitle = nullptr;
    QListWidget *m_roleList = nullptr;
    QPushButton *m_saveButton = nullptr;
    QLabel *m_pendingLabel = nullptr;

    QString m_editorUid;
    QHash<QString, QStringList> m_pending; // uid -> new roles
    bool m_updatingEditor = false;
    bool m_updatingFilters = false;
};

} // namespace fm
