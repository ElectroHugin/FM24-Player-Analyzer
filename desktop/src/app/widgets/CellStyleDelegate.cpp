#include "CellStyleDelegate.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QPainter>

namespace fm {

void CellStyleDelegate::install(QAbstractItemView *view)
{
    view->setItemDelegate(new CellStyleDelegate(view));
}

void CellStyleDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    // Checkboxes / icons: let the base delegate handle them (the stylesheet no
    // longer owns `::item`, so model background colors still come through).
    if (index.data(Qt::CheckStateRole).isValid()
        || !index.data(Qt::DecorationRole).isNull()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const bool selected = opt.state.testFlag(QStyle::State_Selected);
    const bool alternate = opt.features.testFlag(QStyleOptionViewItem::Alternate);
    const QBrush cellBrush = qvariant_cast<QBrush>(index.data(Qt::BackgroundRole));
    const bool hasCellColor = cellBrush.style() != Qt::NoBrush;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    // --- Background ---
    if (hasCellColor) {
        painter->fillRect(opt.rect, cellBrush.color());
        if (selected) {
            // Keep the color visible; mark selection with an accent ring.
            QColor ring = opt.palette.highlight().color();
            ring.setAlpha(230);
            painter->setPen(QPen(ring, 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(opt.rect.adjusted(1, 1, -2, -2));
        }
    } else if (selected) {
        painter->fillRect(opt.rect, opt.palette.highlight());
    } else if (alternate) {
        painter->fillRect(opt.rect, opt.palette.alternateBase());
    } else {
        painter->fillRect(opt.rect, opt.palette.base());
    }

    // --- Text ---
    const QString text = index.data(Qt::DisplayRole).toString();
    if (!text.isEmpty()) {
        // An explicit foreground (e.g. the red "≤17" age warning) always wins;
        // otherwise use the selection/normal text color from the palette.
        const QBrush fgBrush = qvariant_cast<QBrush>(index.data(Qt::ForegroundRole));
        QColor fg;
        if (fgBrush.style() != Qt::NoBrush)
            fg = fgBrush.color();
        else if (hasCellColor)
            fg = QColor(Qt::black);
        else
            fg = selected ? opt.palette.highlightedText().color()
                          : opt.palette.text().color();

        int align = Qt::AlignVCenter | Qt::AlignLeft;
        const QVariant alignData = index.data(Qt::TextAlignmentRole);
        if (alignData.isValid())
            align = alignData.toInt();

        const QRect textRect = opt.rect.adjusted(9, 0, -9, 0);
        const QString elided =
            opt.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width());
        painter->setPen(fg);
        painter->setFont(opt.font);
        painter->drawText(textRect, static_cast<int>(align), elided);
    }

    painter->restore();
}

QSize CellStyleDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QSize hint = QStyledItemDelegate::sizeHint(option, index);
    hint.setHeight(qMax(hint.height() + 8, 30));
    return hint;
}

} // namespace fm
