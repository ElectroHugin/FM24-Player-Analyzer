#include "PlayerActions.h"

#include "AppContext.h"
#include "widgets/PlayerTableModel.h"

#include <QCoreApplication>
#include <QMenu>
#include <QMessageBox>
#include <QTableView>
#include <QTableWidget>

namespace fm {

namespace PlayerActions {

namespace {

QString trActions(const char *text)
{
    return QCoreApplication::translate("PlayerActions", text);
}

// Persists a single mutated store player; rolls back and reports on failure.
template <typename Mutator>
void togglePlayerFlag(AppContext &context, QWidget *parent, const QString &uid,
                      Mutator &&mutate)
{
    const int row = context.store().rowByUid(uid);
    if (row < 0)
        return;
    Player &player = context.store().at(row);
    mutate(player);
    std::vector<Player> batch{player};
    if (!context.database().upsertPlayers(batch)) {
        mutate(player); // toggle back
        QMessageBox::critical(parent, trActions("Spieler"),
                              context.database().errorString());
        return;
    }
    context.notifySettingsChanged(); // dataChanged -> pages refresh
}

void toggleShortlist(AppContext &context, QWidget *parent, const QString &uid)
{
    const int row = context.store().rowByUid(uid);
    if (row < 0)
        return;
    Player &player = context.store().at(row);
    player.onShortlist = !player.onShortlist;
    QList<int> ids;
    for (const Player &p : context.store().players()) {
        if (p.onShortlist)
            ids << p.id;
    }
    if (!context.database().setShortlistIds(ids)) {
        player.onShortlist = !player.onShortlist;
        QMessageBox::critical(parent, trActions("Shortlist"),
                              context.database().errorString());
        return;
    }
    context.notifySettingsChanged();
}

} // namespace

void openProfile(AppContext &context, const QString &uid)
{
    if (uid.isEmpty())
        return;
    context.setPendingProfileUid(uid);
    context.requestNavigation(QStringLiteral("player_profile"));
}

void showContextMenu(AppContext &context, QWidget *parent, const QString &uid,
                     const QPoint &globalPos)
{
    const Player *player = context.store().findByUid(uid);
    if (!player)
        return;

    QMenu menu(parent);
    auto *title = menu.addAction(QStringLiteral("%1 · %2").arg(player->name, player->club));
    title->setEnabled(false);
    menu.addSeparator();

    menu.addAction(trActions("👤 Profil öffnen"), [&context, uid] {
        openProfile(context, uid);
    });
    menu.addAction(trActions("⚖️ Zum Vergleich hinzufügen"), [&context, uid] {
        context.addPendingComparisonUid(uid);
        context.requestNavigation(QStringLiteral("player_comparison"));
    });
    menu.addAction(trActions("✏️ Bearbeiten"), [&context, uid] {
        context.setPendingEditUid(uid);
        context.requestNavigation(QStringLiteral("edit_player"));
    });
    menu.addSeparator();

    auto *transferAction = menu.addAction(trActions("Zum Verkauf anbieten"));
    transferAction->setCheckable(true);
    transferAction->setChecked(player->transferStatus);
    QObject::connect(transferAction, &QAction::triggered, parent,
                     [&context, parent, uid] {
                         togglePlayerFlag(context, parent, uid, [](Player &p) {
                             p.transferStatus = !p.transferStatus;
                         });
                     });

    auto *loanAction = menu.addAction(trActions("Zum Verleih anbieten"));
    loanAction->setCheckable(true);
    loanAction->setChecked(player->loanStatus);
    QObject::connect(loanAction, &QAction::triggered, parent, [&context, parent, uid] {
        togglePlayerFlag(context, parent, uid,
                         [](Player &p) { p.loanStatus = !p.loanStatus; });
    });

    auto *shortlistAction = menu.addAction(trActions("★ Auf der Shortlist"));
    shortlistAction->setCheckable(true);
    shortlistAction->setChecked(player->onShortlist);
    QObject::connect(shortlistAction, &QAction::triggered, parent,
                     [&context, parent, uid] { toggleShortlist(context, parent, uid); });

    menu.exec(globalPos);
}

void attachToView(AppContext &context, QTableView *view, PlayerFilterProxy *proxy,
                  int ignoreDoubleClickColumn)
{
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(view, &QTableView::customContextMenuRequested, view,
                     [&context, view, proxy](const QPoint &pos) {
                         const QModelIndex index = view->indexAt(pos);
                         if (!index.isValid())
                             return;
                         if (const Player *player = proxy->playerAt(index.row())) {
                             showContextMenu(context, view, player->uid,
                                             view->viewport()->mapToGlobal(pos));
                         }
                     });
    QObject::connect(view, &QTableView::doubleClicked, view,
                     [&context, proxy, ignoreDoubleClickColumn](const QModelIndex &index) {
                         if (!index.isValid()
                             || index.column() == ignoreDoubleClickColumn)
                             return;
                         if (const Player *player = proxy->playerAt(index.row()))
                             openProfile(context, player->uid);
                     });
}

void attachToTableWidget(AppContext &context, QTableWidget *table, int uidColumn,
                         bool doubleClickOpensProfile)
{
    const auto uidForRow = [table, uidColumn](int row) {
        const QTableWidgetItem *item = table->item(row, uidColumn);
        return item ? item->data(Qt::UserRole).toString() : QString();
    };
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(table, &QTableWidget::customContextMenuRequested, table,
                     [&context, table, uidForRow](const QPoint &pos) {
                         const QModelIndex index = table->indexAt(pos);
                         if (!index.isValid())
                             return;
                         const QString uid = uidForRow(index.row());
                         if (!uid.isEmpty()) {
                             showContextMenu(context, table, uid,
                                             table->viewport()->mapToGlobal(pos));
                         }
                     });
    if (doubleClickOpensProfile) {
        QObject::connect(table, &QTableWidget::cellDoubleClicked, table,
                         [&context, uidForRow](int row, int) {
                             const QString uid = uidForRow(row);
                             if (!uid.isEmpty())
                                 openProfile(context, uid);
                         });
    }
}

} // namespace PlayerActions

} // namespace fm
