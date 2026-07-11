#pragma once

#include "core/Player.h"
#include "core/Utils.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

#include <functional>
#include <vector>

namespace fm {

// One column of a PlayerTableModel, defined by lambdas so every page can
// compose its own table without a new model class.
struct PlayerColumn {
    QString header;
    // Display value (also used for sorting unless sortValue is set).
    std::function<QVariant(const Player &)> value;
    // Optional numeric/sortable value (Qt::UserRole, proxy sort role).
    std::function<QVariant(const Player &)> sortValue;
    // Optional background/text styling.
    std::function<CellStyle(const Player &)> style;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter;
};

// Read-only table over a set of players (pointers into the PlayerStore; rows
// must be reset whenever the store reloads).
class PlayerTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PlayerTableModel(QObject *parent = nullptr);

    void setColumns(QList<PlayerColumn> columns);
    void setRows(std::vector<const Player *> rows);

    const Player *playerAt(int row) const;
    const std::vector<const Player *> &rows() const { return m_rows; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Exports the current columns for the given (proxy-ordered) players.
    QString toCsv(const std::vector<const Player *> &orderedRows) const;

private:
    QList<PlayerColumn> m_columns;
    std::vector<const Player *> m_rows;
};

// Proxy with case-insensitive name-contains filtering; sorts by Qt::UserRole.
class PlayerFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit PlayerFilterProxy(PlayerTableModel *source, QObject *parent = nullptr);

    void setNameFilter(const QString &text);
    const Player *playerAt(int proxyRow) const;
    std::vector<const Player *> orderedPlayers() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &parent) const override;

private:
    PlayerTableModel *m_model;
    QString m_nameFilter;
};

} // namespace fm
