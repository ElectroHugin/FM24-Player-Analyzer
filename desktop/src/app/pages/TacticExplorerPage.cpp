#include "TacticExplorerPage.h"

#include "../AppContext.h"
#include "PageHelpers.h"
#include "core/Utils.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

const QStringList &stratumOrderList()
{
    static const QStringList order = {
        QStringLiteral("Goalkeeper"),          QStringLiteral("Defense"),
        QStringLiteral("Defensive Midfield"),  QStringLiteral("Midfield"),
        QStringLiteral("Attacking Midfield"),  QStringLiteral("Strikers")};
    return order;
}

QString stratumShort(const QString &stratum)
{
    static const QHash<QString, QString> shorts = {
        {QStringLiteral("Goalkeeper"), QStringLiteral("GK")},
        {QStringLiteral("Defense"), QStringLiteral("DEF")},
        {QStringLiteral("Defensive Midfield"), QStringLiteral("DM")},
        {QStringLiteral("Midfield"), QStringLiteral("MID")},
        {QStringLiteral("Attacking Midfield"), QStringLiteral("AM")},
        {QStringLiteral("Strikers"), QStringLiteral("ST")}};
    return shorts.value(stratum, stratum);
}

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

// DWRS-colored cell for a 0-100 median/mean value; "–" for missing.
NumericItem *dwrsItem(double value)
{
    if (value < 0)
        return new NumericItem(QStringLiteral("–"), -1);
    auto *item = new NumericItem(QString::number(qRound(value)), value);
    const CellStyle style = dwrsCellStyle(value);
    if (style.isValid()) {
        item->setBackground(style.background);
        item->setForeground(style.text);
    }
    return item;
}

} // namespace

TacticExplorerPage::TacticExplorerPage(AppContext &context, QWidget *parent)
    : PageBase(context, parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);
    scroll->setWidget(content);

    auto *heading =
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Taktik-Explorer")), content);
    layout->addWidget(heading);

    auto *info = new QLabel(
        tr("Bewertet deinen Kader gegen JEDE Taktik, um die beste Passung zu finden — "
           "besonders nützlich bei der Übernahme eines unbekannten Teams. Sortiert wird "
           "zuerst nach Abdeckung (wie viele Positionen du besetzen kannst), dann nach "
           "Stärke (Median-DWRS der besten Elf, nur über besetzte Slots gemessen)."),
        content);
    info->setWordWrap(true);
    info->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(info);

    m_poolCaption = new QLabel(content);
    layout->addWidget(m_poolCaption);
    m_bestLabel = new QLabel(content);
    m_bestLabel->setWordWrap(true);
    layout->addWidget(m_bestLabel);

    m_rankTable = new QTableWidget(content);
    const QStringList headers{tr("Taktik"), tr("XI"),     tr("Leer"), tr("Dünn"),
                              tr("Median"), tr("Ø"),      QStringLiteral("GK"),
                              QStringLiteral("DEF"),      QStringLiteral("DM"),
                              QStringLiteral("MID"),      QStringLiteral("AM"),
                              QStringLiteral("ST")};
    m_rankTable->setColumnCount(headers.size());
    m_rankTable->setHorizontalHeaderLabels(headers);
    m_rankTable->verticalHeader()->setVisible(false);
    m_rankTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rankTable->setAlternatingRowColors(true);
    m_rankTable->setMinimumHeight(320);
    layout->addWidget(m_rankTable);
    auto *legend = new QLabel(
        tr("GK/DEF/DM/MID/AM/ST = Median-DWRS je Mannschaftsteil. Leer = Slots, die die "
           "beste Elf nicht besetzen konnte · Dünn = nur ein geeigneter Spieler (kein "
           "Backup)."),
        content);
    legend->setWordWrap(true);
    legend->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(legend);

    auto *divider = new QFrame(content);
    divider->setFrameShape(QFrame::HLine);
    layout->addWidget(divider);

    auto *detailTitle = new QLabel(tr("Taktik im Detail"), content);
    detailTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(detailTitle);
    auto *detailRow = new QHBoxLayout;
    detailRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_detailCombo = new QComboBox(content);
    m_detailCombo->setMinimumWidth(260);
    detailRow->addWidget(m_detailCombo);
    detailRow->addStretch(1);
    layout->addLayout(detailRow);

    m_detailMetrics = new QLabel(content);
    layout->addWidget(m_detailMetrics);
    m_detailWarnings = new QLabel(content);
    m_detailWarnings->setWordWrap(true);
    layout->addWidget(m_detailWarnings);

    m_depthTable = new QTableWidget(content);
    m_depthTable->setColumnCount(3);
    m_depthTable->setHorizontalHeaderLabels(
        {tr("Slot"), tr("Rolle"), tr("Geeignete Spieler")});
    m_depthTable->verticalHeader()->setVisible(false);
    m_depthTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_depthTable->setAlternatingRowColors(true);
    m_depthTable->setSortingEnabled(true);
    m_depthTable->setMinimumHeight(280);
    layout->addWidget(m_depthTable);
    layout->addStretch(1);

    // Selecting a row in the ranking jumps to the detail view.
    connect(m_rankTable, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
                if (m_updating || row < 0)
                    return;
                const QTableWidgetItem *item = m_rankTable->item(row, 0);
                if (item)
                    m_detailCombo->setCurrentText(item->text());
            });
    connect(m_detailCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuildDetail();
    });
}

void TacticExplorerPage::refresh()
{
    m_updating = true;
    const bool national = m_context.nationalUiMode();

    // --- Pool ---
    std::vector<const Player *> pool;
    QString poolLabel;
    bool applyApt = true;
    if (national) {
        // National mode: the saved squad, and club APT must not skew selection.
        applyApt = false;
        for (const Player &player : m_context.store().players()) {
            if (player.inNationalSquad)
                pool.push_back(&player);
        }
        poolLabel = m_context.nationalTeamName().isEmpty()
                        ? tr("Nationalkader")
                        : tr("%1-Kader").arg(m_context.nationalTeamName());
        if (pool.empty()) {
            m_poolCaption->setText(
                tr("Noch keine Spieler im Nationalkader. Stelle ihn unter "
                   "'Kader-Auswahl' zusammen."));
            m_bestLabel->clear();
            m_rankTable->setRowCount(0);
            m_detailCombo->clear();
            m_results.clear();
            m_updating = false;
            rebuildDetail();
            return;
        }
    } else {
        const QString userClub = m_context.userClub();
        const QString secondClub = m_context.secondTeamClub();
        if (userClub.isEmpty()) {
            m_poolCaption->setText(
                tr("Bitte wähle zuerst deinen Verein (Dashboard oder Einstellungen)."));
            m_bestLabel->clear();
            m_rankTable->setRowCount(0);
            m_detailCombo->clear();
            m_results.clear();
            m_updating = false;
            rebuildDetail();
            return;
        }
        for (const Player &player : m_context.store().players()) {
            if (player.club == userClub
                || (!secondClub.isEmpty() && player.club == secondClub))
                pool.push_back(&player);
        }
        poolLabel = userClub;
        if (!secondClub.isEmpty())
            poolLabel += QStringLiteral(" + ") + secondClub;
    }

    m_results = m_context.tacticExplorer().analyzeAllTactics(pool, m_context.ratings(), {},
                                                             applyApt);
    m_poolCaption->setText(tr("Pool: %1 Spieler (%2).").arg(pool.size()).arg(poolLabel));

    if (m_results.empty()) {
        m_bestLabel->clear();
        m_rankTable->setRowCount(0);
        m_detailCombo->clear();
        m_updating = false;
        rebuildDetail();
        return;
    }

    const TacticMetrics &best = m_results.front();
    m_bestLabel->setText(
        tr("🏆 <b>Beste Passung: %1</b> — besetzt %2/%3 Positionen, Median-DWRS %4.")
            .arg(best.tactic.toHtmlEscaped())
            .arg(best.filledSlots)
            .arg(best.totalSlots)
            .arg(best.overallMedian >= 0
                     ? QStringLiteral("%1%").arg(qRound(best.overallMedian))
                     : QStringLiteral("–")));

    // --- Ranking table (kept in explorer order: coverage first, then median).
    m_rankTable->setSortingEnabled(false);
    m_rankTable->setRowCount(static_cast<int>(m_results.size()));
    int row = 0;
    for (const TacticMetrics &result : m_results) {
        m_rankTable->setItem(row, 0, new QTableWidgetItem(result.tactic));
        m_rankTable->setItem(row, 1,
                             new NumericItem(QStringLiteral("%1/%2")
                                                 .arg(result.filledSlots)
                                                 .arg(result.totalSlots),
                                             result.filledSlots));
        const auto warnItem = [](int count) {
            auto *item = new NumericItem(QString::number(count), count);
            if (count > 0)
                item->setBackground(QColor(207, 30, 30, 140));
            return item;
        };
        m_rankTable->setItem(row, 2, warnItem(static_cast<int>(result.emptySlots.size())));
        m_rankTable->setItem(row, 3, warnItem(static_cast<int>(result.thinSlots.size())));
        m_rankTable->setItem(row, 4, dwrsItem(result.overallMedian));
        m_rankTable->setItem(row, 5, dwrsItem(result.overallMean));
        int column = 6;
        for (const QString &stratum : stratumOrderList()) {
            const auto it = result.perStratum.constFind(stratum);
            m_rankTable->setItem(row, column++,
                                 dwrsItem(it != result.perStratum.constEnd()
                                              ? it.value().median
                                              : -1.0));
        }
        ++row;
    }
    m_rankTable->resizeColumnsToContents();

    const QString previousDetail = m_detailCombo->currentText();
    m_detailCombo->clear();
    for (const TacticMetrics &result : m_results)
        m_detailCombo->addItem(result.tactic);
    if (!previousDetail.isEmpty() && m_detailCombo->findText(previousDetail) >= 0)
        m_detailCombo->setCurrentText(previousDetail);

    m_updating = false;
    rebuildDetail();
}

void TacticExplorerPage::rebuildDetail()
{
    const QString tactic = m_detailCombo->currentText();
    const auto it = std::find_if(m_results.cbegin(), m_results.cend(),
                                 [&tactic](const TacticMetrics &metrics) {
                                     return metrics.tactic == tactic;
                                 });
    if (it == m_results.cend()) {
        m_detailMetrics->clear();
        m_detailWarnings->clear();
        m_depthTable->setRowCount(0);
        return;
    }
    const TacticMetrics &result = *it;
    const auto roleNames = m_context.definitions().roleDisplayMap();

    m_detailMetrics->setText(
        tr("<b>Besetzte Positionen:</b> %1/%2 &nbsp;·&nbsp; <b>Median-DWRS:</b> %3 "
           "&nbsp;·&nbsp; <b>Ø Optionen pro Slot:</b> %4")
            .arg(result.filledSlots)
            .arg(result.totalSlots)
            .arg(result.overallMedian >= 0
                     ? QStringLiteral("%1%").arg(qRound(result.overallMedian))
                     : QStringLiteral("–"))
            .arg(result.avgDepth, 0, 'f', 1));

    const auto slotList = [&](const QStringList &slotNames) {
        QStringList parts;
        for (const QString &slot : slotNames) {
            const QString role = result.positions.value(slot);
            parts << QStringLiteral("%1 (%2)").arg(slot, roleNames.value(role, role));
        }
        return parts.join(QStringLiteral(", "));
    };
    QStringList warnings;
    if (!result.uncoverableSlots.isEmpty())
        warnings << tr("⛔ <b>Gar kein geeigneter Spieler</b> für: %1 — für diese Formation "
                       "fehlen dir Spielertypen.")
                        .arg(slotList(result.uncoverableSlots));
    QStringList onlyXiEmpty;
    for (const QString &slot : result.emptySlots) {
        if (!result.uncoverableSlots.contains(slot))
            onlyXiEmpty << slot;
    }
    if (!onlyXiEmpty.isEmpty())
        warnings << tr("⚠️ <b>In der besten Elf leer geblieben</b> (Spieler existieren, "
                       "werden aber woanders gebraucht): %1")
                        .arg(slotList(onlyXiEmpty));
    if (!result.thinSlots.isEmpty())
        warnings << tr("<b>Nur eine Option (kein Backup):</b> %1")
                        .arg(slotList(result.thinSlots));
    m_detailWarnings->setText(warnings.join(QStringLiteral("<br/>")));

    m_depthTable->setSortingEnabled(false);
    m_depthTable->setRowCount(static_cast<int>(result.positions.size()));
    int row = 0;
    for (auto posIt = result.positions.constBegin(); posIt != result.positions.constEnd();
         ++posIt) {
        m_depthTable->setItem(row, 0, new QTableWidgetItem(posIt.key()));
        m_depthTable->setItem(
            row, 1, new QTableWidgetItem(roleNames.value(posIt.value(), posIt.value())));
        const int count = result.eligibleCounts.value(posIt.key(), 0);
        auto *countItem = new NumericItem(QString::number(count), count);
        if (count == 0)
            countItem->setBackground(QColor(207, 30, 30, 140));
        else if (count == 1)
            countItem->setBackground(QColor(216, 210, 30, 115));
        else
            countItem->setBackground(QColor(13, 160, 37, 90));
        m_depthTable->setItem(row, 2, countItem);
        ++row;
    }
    m_depthTable->setSortingEnabled(true);
    m_depthTable->resizeColumnsToContents();
}

} // namespace fm
