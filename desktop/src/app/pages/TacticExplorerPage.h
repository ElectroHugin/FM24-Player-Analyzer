#pragma once

#include "PageBase.h"

#include "core/TacticExplorer.h"

#include <vector>

class QComboBox;
class QLabel;
class QTableWidget;

namespace fm {

// Ranks every tactic by squad coverage, then strength; with a per-tactic
// detail view. Mode-aware: club pool (user + second team) or the national
// squad (without APT weighting). Port of legacy tactic_explorer.py.
class TacticExplorerPage : public PageBase
{
    Q_OBJECT

public:
    explicit TacticExplorerPage(AppContext &context, QWidget *parent = nullptr);

    void refresh() override;

private:
    void rebuildDetail();

    QLabel *m_poolCaption = nullptr;
    QLabel *m_bestLabel = nullptr;
    QTableWidget *m_rankTable = nullptr;
    QComboBox *m_detailCombo = nullptr;
    QLabel *m_detailMetrics = nullptr;
    QLabel *m_detailWarnings = nullptr;
    QTableWidget *m_depthTable = nullptr;

    std::vector<TacticMetrics> m_results;
    bool m_updating = false;
};

} // namespace fm
