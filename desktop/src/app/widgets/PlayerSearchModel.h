#pragma once

#include "core/Utils.h" // foldForSearch

#include <QAbstractListModel>
#include <QCompleter>
#include <QString>
#include <QStringList>

#include <vector>

namespace fm {

// Matching happens against a folded (accent-/umlaut-insensitive) key; the popup
// still shows the pretty display string.
constexpr int PlayerSearchKeyRole = Qt::UserRole + 1;

// Compact list model backing a player-search completer: one small struct per
// player instead of a heap-allocated QStandardItem (the sidebar covers ~80k
// players), with the folded search key precomputed once at build time.
class PlayerSearchModel : public QAbstractListModel
{
public:
    struct Entry {
        QString display; // shown in the popup
        QString uid;     // Qt::UserRole payload
        QString key;     // folded match key (PlayerSearchKeyRole)
    };

    using QAbstractListModel::QAbstractListModel;

    void setEntries(std::vector<Entry> entries)
    {
        beginResetModel();
        m_entries = std::move(entries);
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
            return {};
        const Entry &e = m_entries[static_cast<size_t>(index.row())];
        switch (role) {
        case Qt::DisplayRole:
            return e.display;
        case Qt::UserRole:
            return e.uid;
        case PlayerSearchKeyRole:
            return e.key;
        default:
            return {};
        }
    }

private:
    std::vector<Entry> m_entries;
};

// A completer whose typed query is folded the same way as the model keys, so
// "Muller" matches "Müller" and vice versa.
class FoldingCompleter : public QCompleter
{
public:
    using QCompleter::QCompleter;
    QStringList splitPath(const QString &path) const override
    {
        return {foldForSearch(path)};
    }
};

} // namespace fm
