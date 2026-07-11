#include "PlayerTableModel.h"

namespace fm {

PlayerTableModel::PlayerTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void PlayerTableModel::setColumns(QList<PlayerColumn> columns)
{
    beginResetModel();
    m_columns = std::move(columns);
    endResetModel();
}

void PlayerTableModel::setRows(std::vector<const Player *> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

const Player *PlayerTableModel::playerAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return nullptr;
    return m_rows[static_cast<size_t>(row)];
}

int PlayerTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int PlayerTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_columns.size());
}

QVariant PlayerTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_rows.size())
        || index.column() >= m_columns.size())
        return {};

    const Player &player = *m_rows[static_cast<size_t>(index.row())];
    const PlayerColumn &column = m_columns.at(index.column());

    switch (role) {
    case Qt::DisplayRole:
        return column.value ? column.value(player) : QVariant();
    case Qt::UserRole:
        if (column.sortValue)
            return column.sortValue(player);
        return column.value ? column.value(player) : QVariant();
    case Qt::BackgroundRole:
        if (column.style) {
            const CellStyle style = column.style(player);
            if (style.isValid())
                return style.background;
        }
        return {};
    case Qt::ForegroundRole:
        if (column.style) {
            const CellStyle style = column.style(player);
            if (style.isValid())
                return style.text;
        }
        return {};
    case Qt::TextAlignmentRole:
        return QVariant::fromValue(column.alignment);
    default:
        return {};
    }
}

QVariant PlayerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < m_columns.size())
        return m_columns.at(section).header;
    return QAbstractTableModel::headerData(section, orientation, role);
}

QString PlayerTableModel::toCsv(const std::vector<const Player *> &orderedRows) const
{
    const auto quote = [](QString value) {
        if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"'))
            || value.contains(QLatin1Char('\n'))) {
            value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            return QStringLiteral("\"%1\"").arg(value);
        }
        return value;
    };

    QStringList lines;
    QStringList header;
    for (const PlayerColumn &column : m_columns)
        header << quote(column.header);
    lines << header.join(QLatin1Char(','));

    for (const Player *player : orderedRows) {
        QStringList cells;
        for (const PlayerColumn &column : m_columns)
            cells << quote(column.value ? column.value(*player).toString() : QString());
        lines << cells.join(QLatin1Char(','));
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

PlayerFilterProxy::PlayerFilterProxy(PlayerTableModel *source, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_model(source)
{
    setSourceModel(source);
    setSortRole(Qt::UserRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void PlayerFilterProxy::setNameFilter(const QString &text)
{
    m_nameFilter = text.trimmed();
    invalidateRowsFilter();
}

const Player *PlayerFilterProxy::playerAt(int proxyRow) const
{
    const QModelIndex source = mapToSource(index(proxyRow, 0));
    return m_model->playerAt(source.row());
}

std::vector<const Player *> PlayerFilterProxy::orderedPlayers() const
{
    std::vector<const Player *> result;
    result.reserve(static_cast<size_t>(rowCount()));
    for (int row = 0; row < rowCount(); ++row) {
        if (const Player *player = playerAt(row))
            result.push_back(player);
    }
    return result;
}

bool PlayerFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (m_nameFilter.isEmpty())
        return true;
    const Player *player = m_model->playerAt(sourceRow);
    return player && player->name.contains(m_nameFilter, Qt::CaseInsensitive);
}

} // namespace fm
