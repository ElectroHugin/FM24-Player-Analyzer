#pragma once

#include <QWidget>

namespace fm {

class AppContext;

// Base for all navigable pages. Pages are constructed lazily on first
// activation; refresh() is called when the page becomes visible or when
// AppContext::dataChanged() fires while it is visible.
class PageBase : public QWidget
{
    Q_OBJECT

public:
    explicit PageBase(AppContext &context, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_context(context)
    {
    }

    virtual void refresh() {}

    // Called on every non-visible page when the player store is replaced (a
    // reload), so no page keeps dangling Player* into the old store. Pages that
    // cache raw Player* (table models, filter pools) override this to drop them;
    // they rebuild in refresh() when shown again. The visible page is refreshed
    // instead, so it is not asked to release.
    virtual void releaseStoreRows() {}

protected:
    AppContext &m_context;
};

} // namespace fm
