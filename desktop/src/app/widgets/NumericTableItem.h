#pragma once

#include <QTableWidgetItem>

namespace fm {

// Table item that displays text but sorts by a numeric key, so columns like
// age/DWRS/value sort by magnitude instead of lexicographically. Shared by the
// table pages (previously duplicated in each one).
class NumericItem : public QTableWidgetItem
{
public:
    NumericItem(const QString &text, double sortKey)
        : QTableWidgetItem(text)
        , m_key(sortKey)
    {
    }
    bool operator<(const QTableWidgetItem &other) const override
    {
        if (const auto *numeric = dynamic_cast<const NumericItem *>(&other))
            return m_key < numeric->m_key;
        return QTableWidgetItem::operator<(other);
    }

private:
    double m_key;
};

} // namespace fm
