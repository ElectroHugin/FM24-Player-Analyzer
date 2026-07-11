#include "GapAnalysisPage.h"

#include "../AppContext.h"
#include "../widgets/TacticPitchWidget.h"
#include "PageHelpers.h"
#include "core/GapAnalysis.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

QString gapTypeLabel(const Gap &gap)
{
    if (gap.playerName == QLatin1String("—") || gap.playerName == QLatin1String("-"))
        return QObject::tr("⛔ Unbesetzt");
    if (gap.isDisplacement && gap.isDropoff)
        return QObject::tr("🕳️⚠️ Versteckt + Offensichtlich");
    if (gap.isDisplacement)
        return QObject::tr("🕳️ Versteckt");
    if (gap.isDropoff)
        return QObject::tr("⚠️ Offensichtlich");
    return QString();
}

// Numeric-sorting table item.
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

} // namespace

GapAnalysisPage::GapAnalysisPage(AppContext &context, ThemeManager &theme, QWidget *parent)
    : PageBase(context, parent)
    , m_theme(theme)
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

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Gap-Analyse")), content);
    layout->addWidget(heading);

    auto *info = new QLabel(
        tr("Findet Schwachstellen in Startelf und B-Team — sowohl offensichtliche Lücken "
           "(Stammspieler deutlich unter dem Team-Median) als auch versteckte (ein guter "
           "Spieler wird aus seinem besten Slot gezogen oder spielt auf der falschen "
           "Seite). So verstärkst du gezielt, statt eine Position zu kaufen, die nur "
           "schwach aussieht."),
        content);
    info->setWordWrap(true);
    info->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(info);

    auto *tacticRow = new QHBoxLayout;
    tacticRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_tacticCombo = new QComboBox(content);
    m_tacticCombo->setMinimumWidth(260);
    tacticRow->addWidget(m_tacticCombo);
    tacticRow->addStretch(1);
    layout->addLayout(tacticRow);

    m_thresholdCaption = new QLabel(content);
    m_thresholdCaption->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(m_thresholdCaption);

    m_tabs = new QTabWidget(content);
    m_tabs->addTab(buildTeamTab(&m_xiView), tr("🏆 Startelf"));
    m_tabs->addTab(buildTeamTab(&m_bTeamView), tr("🅱️ B-Team"));
    layout->addWidget(m_tabs, 1);

    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuild();
    });
}

QWidget *GapAnalysisPage::buildTeamTab(TeamView *view)
{
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    view->status = new QLabel(tab);
    view->status->setWordWrap(true);
    layout->addWidget(view->status);

    view->table = new QTableWidget(tab);
    view->table->setColumnCount(7);
    view->table->setHorizontalHeaderLabels({tr("Slot"), tr("Rolle"), tr("Spieler"),
                                            tr("DWRS"), tr("Typ"), tr("Gap-Score"),
                                            tr("Warum")});
    view->table->verticalHeader()->setVisible(false);
    view->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->table->setAlternatingRowColors(true);
    view->table->setSortingEnabled(true);
    view->table->setMinimumHeight(260);
    view->table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(view->table);

    auto *pitchBox = new QGroupBox(tr("🟩 Auf dem Platz anzeigen"), tab);
    pitchBox->setCheckable(true);
    pitchBox->setChecked(false);
    auto *pitchLayout = new QVBoxLayout(pitchBox);
    view->pitch = new TacticPitchWidget(m_theme, pitchBox);
    view->pitch->setVisible(false);
    pitchLayout->addWidget(view->pitch, 0, Qt::AlignHCenter);
    connect(pitchBox, &QGroupBox::toggled, view->pitch, &QWidget::setVisible);
    layout->addWidget(pitchBox);
    layout->addStretch(1);
    return tab;
}

void GapAnalysisPage::refresh()
{
    m_updating = true;
    const QString previous = m_tacticCombo->currentText();
    m_tacticCombo->clear();
    m_tacticCombo->addItems(favoritesFirstTactics(m_context, false));
    if (!previous.isEmpty() && m_tacticCombo->findText(previous) >= 0)
        m_tacticCombo->setCurrentText(previous);
    m_updating = false;
    rebuild();
}

void GapAnalysisPage::rebuild()
{
    const QString userClub = m_context.userClub();
    const QString tactic = m_tacticCombo->currentText();
    if (userClub.isEmpty() || tactic.isEmpty()) {
        m_xiView.status->setText(
            tr("Bitte wähle zuerst deinen Verein (Dashboard oder Einstellungen)."));
        m_bTeamView.status->setText(QString());
        m_xiView.table->setRowCount(0);
        m_bTeamView.table->setRowCount(0);
        return;
    }

    AppConfig &config = m_context.config();
    const double displacement =
        config.gapAnalysisSetting(QStringLiteral("displacement_threshold"));
    const double dropoff = config.gapAnalysisSetting(QStringLiteral("dropoff_threshold"));
    const double wrongSide = config.gapAnalysisSetting(QStringLiteral("wrong_side_penalty"));
    m_thresholdCaption->setText(
        tr("Schwellen — Versteckt: %1 · Offensichtlich: %2 · Falsche-Seite-Malus: %3 "
           "(anpassbar unter Einstellungen → Schwellenwerte)")
            .arg(displacement, 0, 'f', 1)
            .arg(dropoff, 0, 'f', 1)
            .arg(wrongSide, 0, 'f', 1));

    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    const QStringList slotOrder = m_context.definitions().tacticSlotOrder(tactic);
    const RoleRatings &ratings = m_context.ratings();

    std::vector<const Player *> clubPlayers;
    QHash<QString, const Player *> playersByUid;
    for (const Player &player : m_context.store().players()) {
        if (player.club == userClub) {
            clubPlayers.push_back(&player);
            playersByUid.insert(player.uid, &player);
        }
    }

    const SquadResult squad = m_context.squadBuilder().calculateSquadAndSurplus(
        clubPlayers, positions, slotOrder, ratings);

    const auto layout = m_context.definitions().tacticLayouts().value(tactic);
    const auto roleNames = m_context.definitions().roleDisplayMap();

    const auto analyze = [&](const QHash<QString, XiCell> &team, TeamView &view) {
        const std::vector<Gap> gaps = GapAnalysis::analyzeTeamGaps(
            team, positions, slotOrder, playersByUid, ratings, displacement, dropoff,
            wrongSide);
        showGaps(view, gaps);
        // Heat pitch: red = big gap, green = fine.
        view.pitch->setTeam(team, positions, layout, roleNames);
        QHash<QString, QColor> colors;
        double maxScore = 0.0;
        for (const Gap &gap : gaps)
            maxScore = std::max(maxScore, gap.gapScore);
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
            colors.insert(it.key(), QColor(40, 120, 60, 217)); // green default
        for (const Gap &gap : gaps) {
            const double norm = maxScore > 0.0 ? gap.gapScore / maxScore : 0.0;
            colors.insert(gap.slot, QColor(static_cast<int>(40 + norm * 180),
                                           static_cast<int>(120 - norm * 90), 50, 230));
        }
        view.pitch->setSlotColors(colors);
    };
    analyze(squad.startingXi, m_xiView);
    analyze(squad.bTeam, m_bTeamView);
}

void GapAnalysisPage::showGaps(TeamView &view, const std::vector<Gap> &gaps)
{
    const auto roleNames = m_context.definitions().roleDisplayMap();

    if (gaps.empty()) {
        view.status->setText(tr("✅ Keine nennenswerten Lücken mit den aktuellen "
                                "Schwellenwerten gefunden."));
        view.table->setRowCount(0);
        return;
    }
    view.status->setText(tr("%1 auffällige Position(en) gefunden — schlechteste zuerst.")
                             .arg(gaps.size()));

    double maxScore = 0.0;
    for (const Gap &gap : gaps)
        maxScore = std::max(maxScore, gap.gapScore);

    view.table->setSortingEnabled(false);
    view.table->setRowCount(static_cast<int>(gaps.size()));
    int row = 0;
    for (const Gap &gap : gaps) {
        view.table->setItem(row, 0, new QTableWidgetItem(gap.slot));
        view.table->setItem(row, 1,
                            new QTableWidgetItem(roleNames.value(gap.role, gap.role)));
        view.table->setItem(row, 2, new QTableWidgetItem(gap.playerName));
        view.table->setItem(row, 3,
                            new NumericItem(QString::number(qRound(gap.assignedDwrs)),
                                            gap.assignedDwrs));
        view.table->setItem(row, 4, new QTableWidgetItem(gapTypeLabel(gap)));
        auto *scoreItem = new NumericItem(QString::number(gap.gapScore, 'f', 1), gap.gapScore);
        const double norm = maxScore > 0.0 ? gap.gapScore / maxScore : 0.0;
        scoreItem->setBackground(QColor(static_cast<int>(200 * norm + 40),
                                        static_cast<int>(160 * (1 - norm) + 40), 50, 140));
        view.table->setItem(row, 5, scoreItem);
        view.table->setItem(row, 6, new QTableWidgetItem(gap.reason));
        ++row;
    }
    view.table->setSortingEnabled(true);
    view.table->sortByColumn(5, Qt::DescendingOrder);
    view.table->resizeColumnsToContents();
}

} // namespace fm
