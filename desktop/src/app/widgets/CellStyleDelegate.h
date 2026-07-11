#pragma once

#include <QStyledItemDelegate>

class QAbstractItemView;

namespace fm {

// Draws table cells itself so the Qt::BackgroundRole / Qt::ForegroundRole
// coming from the model (DWRS gist-rainbow colors, attribute tiers,
// personality) survive a global stylesheet. A `QTableView::item {}` QSS rule
// makes the stylesheet renderer own item painting and silently drop those
// model colors — this delegate bypasses that entirely.
//
// Cells that carry a check state or an icon fall back to the base delegate so
// checkboxes/decorations keep working; every other cell is painted with a
// comfortable padding and, for colored cells, an accent selection ring that
// keeps the fill visible.
class CellStyleDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // Installs a delegate instance (parented to the view) on the view.
    static void install(QAbstractItemView *view);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

} // namespace fm
