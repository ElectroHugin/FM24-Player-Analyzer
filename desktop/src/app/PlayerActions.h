#pragma once

#include <QPoint>
#include <QString>

class QTableView;
class QTableWidget;
class QWidget;

namespace fm {

class AppContext;
class PlayerFilterProxy;

// Shared player quick actions (M13): context menu + double-click behavior for
// every table and pitch that shows players. Not possible in the Streamlit app.
namespace PlayerActions {

// Jumps to the profile page for the player.
void openProfile(AppContext &context, const QString &uid);

// Shows the full context menu (profile / comparison / edit / transfer / loan /
// shortlist toggles) at the given global position.
void showContextMenu(AppContext &context, QWidget *parent, const QString &uid,
                     const QPoint &globalPos);

// Wires a PlayerTableModel-backed view: right-click menu + double-click
// profile. ignoreDoubleClickColumn: column whose double-click is left alone
// (e.g. the shortlist star).
void attachToView(AppContext &context, QTableView *view, PlayerFilterProxy *proxy,
                  int ignoreDoubleClickColumn = -1);

// Wires a QTableWidget whose rows carry the player uid in
// item(row, uidColumn)->data(Qt::UserRole). doubleClickOpensProfile = false
// for editable tables where double-click must keep starting the editor.
void attachToTableWidget(AppContext &context, QTableWidget *table, int uidColumn = 0,
                         bool doubleClickOpensProfile = true);

} // namespace PlayerActions

} // namespace fm
