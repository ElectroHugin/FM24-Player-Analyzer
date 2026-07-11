#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

namespace fm {

class AppContext;

// Recalculates DWRS for exactly the given players in a background thread with
// a modal progress dialog. The caller must have PERSISTED its player changes
// (and refreshed the in-memory store) beforehand — the worker snapshots the
// store. On success the context is reloaded (dataChanged() fires);
// onDone(errorMessage) runs afterwards with an empty string on success.
void recalcDwrsFor(AppContext &context, QWidget *parent, const QStringList &affectedUids,
                   std::function<void(QString)> onDone = {});

} // namespace fm
