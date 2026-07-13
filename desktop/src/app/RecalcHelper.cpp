#include "RecalcHelper.h"

#include "AppContext.h"
#include "core/RatingsUpdater.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QPointer>
#include <QProgressDialog>
#include <QSet>
#include <QtConcurrentRun>

namespace fm {

void recalcDwrsFor(AppContext &context, QWidget *parent, const QStringList &affectedUids,
                   std::function<void(QString)> onDone)
{
    auto *dialog = new QProgressDialog(QObject::tr("DWRS-Bewertungen werden berechnet…"),
                                       QString(), 0, 100, parent);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setValue(0);

    const QString dbFile = context.database().filePath();
    const QStringList validRoles = context.definitions().validRoles();
    const DwrsEngine *engine = &context.dwrsEngine();
    std::vector<Player> players = context.store().players();
    const QSet<QString> affected(affectedUids.cbegin(), affectedUids.cend());

    auto *watcher = new QFutureWatcher<RatingsUpdater::Result>(parent);
    QObject::connect(watcher, &QFutureWatcher<RatingsUpdater::Result>::finished, parent,
                     [watcher, dialog, &context, onDone = std::move(onDone)] {
                         const RatingsUpdater::Result result = watcher->result();
                         watcher->deleteLater();
                         dialog->close();
                         dialog->deleteLater();
                         if (result.success)
                             context.reloadFromDatabase();
                         if (onDone)
                             onDone(result.success ? QString() : result.error);
                     });

    watcher->setFuture(QtConcurrent::run(
        [dbFile, players = std::move(players), engine, validRoles, affected,
         dialogGuard = QPointer<QProgressDialog>(dialog)] {
            Database db(QStringLiteral("recalc_helper"));
            if (!db.open(dbFile)) {
                RatingsUpdater::Result result;
                result.error = db.errorString();
                return result;
            }
            std::vector<int> subset;
            subset.reserve(affected.size());
            for (size_t i = 0; i < players.size(); ++i) {
                if (affected.contains(players[i].uid))
                    subset.push_back(static_cast<int>(i));
            }
            return RatingsUpdater::updateDwrsRatings(
                db, players, *engine, validRoles, subset,
                [dialogGuard](int current, int total) {
                    const int percent = total > 0 ? current * 100 / total : 0;
                    QMetaObject::invokeMethod(
                        qApp,
                        [dialogGuard, percent] {
                            if (dialogGuard)
                                dialogGuard->setValue(percent);
                        },
                        Qt::QueuedConnection);
                });
        }));
}

} // namespace fm
