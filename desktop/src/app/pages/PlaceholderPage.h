#pragma once

#include "PageBase.h"

#include <QLabel>
#include <QVBoxLayout>

namespace fm {

// Defensive fallback shown by MainWindow::createPage() for an unknown page id.
// All real menu pages are wired directly, so this is not reached in normal use.
class PlaceholderPage : public PageBase
{
    Q_OBJECT

public:
    PlaceholderPage(AppContext &context, const QString &title, QWidget *parent = nullptr)
        : PageBase(context, parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->addStretch(1);
        auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(title), this);
        heading->setAlignment(Qt::AlignCenter);
        auto *note = new QLabel(tr("Diese Seite ist nicht verfügbar."), this);
        note->setAlignment(Qt::AlignCenter);
        layout->addWidget(heading);
        layout->addWidget(note);
        layout->addStretch(2);
    }
};

} // namespace fm
