#pragma once

#include "PageBase.h"

#include <QLabel>
#include <QVBoxLayout>

namespace fm {

// Temporary stand-in for pages that are ported in later milestones.
class PlaceholderPage : public PageBase
{
    Q_OBJECT

public:
    PlaceholderPage(AppContext &context, const QString &title, const QString &milestone,
                    QWidget *parent = nullptr)
        : PageBase(context, parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->addStretch(1);
        auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(title), this);
        heading->setAlignment(Qt::AlignCenter);
        auto *note = new QLabel(
            tr("Diese Seite wird in Meilenstein %1 portiert.").arg(milestone), this);
        note->setAlignment(Qt::AlignCenter);
        layout->addWidget(heading);
        layout->addWidget(note);
        layout->addStretch(2);
    }
};

} // namespace fm
