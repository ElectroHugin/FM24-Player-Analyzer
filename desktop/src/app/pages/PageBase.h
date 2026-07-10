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

protected:
    AppContext &m_context;
};

} // namespace fm
