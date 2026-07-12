#include "BestXiPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "../widgets/TacticPitchWidget.h"
#include "PageHelpers.h"
#include "core/TalentEngine.h"
#include "core/Utils.h"

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

// Table item that displays text but sorts by a numeric key.
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

BestXiPage::BestXiPage(AppContext &context, ThemeManager &theme, QWidget *parent)
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

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Best XI")), content);
    layout->addWidget(heading);

    auto *tacticRow = new QHBoxLayout;
    tacticRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_tacticCombo = new QComboBox(content);
    m_tacticCombo->setMinimumWidth(260);
    tacticRow->addWidget(m_tacticCombo);
    tacticRow->addStretch(1);
    layout->addLayout(tacticRow);

    m_hint = new QLabel(
        tr("Der Rechner nutzt einen 'Schwächstes-Glied-zuerst'-Algorithmus: Statt Position "
           "für Position den besten Spieler zu wählen, wird immer zuerst die Position "
           "besetzt, auf der der beste verfügbare Spieler die geringste Verbesserung "
           "bringt — das ergibt insgesamt ausgewogenere und stärkere Teams."),
        content);
    m_hint->setWordWrap(true);
    m_hint->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(m_hint);

    m_tabs = new QTabWidget(content);

    // --- Tab 1: first team ---
    {
        auto *tab = new QWidget;
        auto *tabLayout = new QVBoxLayout(tab);
        auto *pitchRow = new QHBoxLayout;
        auto *xiColumn = new QVBoxLayout;
        auto *xiTitle = new QLabel(tr("Startelf"), tab);
        xiTitle->setObjectName(QStringLiteral("sectionTitle"));
        xiTitle->setAlignment(Qt::AlignHCenter);
        m_xiPitch = new TacticPitchWidget(m_theme, tab);
        xiColumn->addWidget(xiTitle);
        xiColumn->addWidget(m_xiPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
        auto *bColumn = new QVBoxLayout;
        auto *bTitle = new QLabel(tr("B-Team"), tab);
        bTitle->setObjectName(QStringLiteral("sectionTitle"));
        bTitle->setAlignment(Qt::AlignHCenter);
        m_bTeamPitch = new TacticPitchWidget(m_theme, tab);
        bColumn->addWidget(bTitle);
        bColumn->addWidget(m_bTeamPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
        pitchRow->addLayout(xiColumn, 1);
        pitchRow->addLayout(bColumn, 1);
        tabLayout->addLayout(pitchRow);

        m_depthBox = new QGroupBox(tr("Weitere Kader-Tiefe"), tab);
        auto *depthLayout = new QVBoxLayout(m_depthBox);
        m_depthEmpty = new QLabel(m_depthBox);
        m_depthEmpty->setWordWrap(true);
        m_depthEmpty->setObjectName(QStringLiteral("kpiCaption"));
        depthLayout->addWidget(m_depthEmpty);
        m_depthTable = new QTableWidget(m_depthBox);
        m_depthTable->setColumnCount(5);
        m_depthTable->setHorizontalHeaderLabels(
            {tr("Rolle"), tr("Name"), tr("Alter"), tr("DWRS"), tr("Eignung")});
        m_depthTable->verticalHeader()->setVisible(false);
        m_depthTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_depthTable->setAlternatingRowColors(true);
        m_depthTable->setSortingEnabled(true);
        m_depthTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_depthTable->horizontalHeader()->setStretchLastSection(true);
        m_depthTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_depthTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_depthTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        depthLayout->addWidget(m_depthTable);
        tabLayout->addWidget(m_depthBox);
        tabLayout->addStretch(1);
        m_tabs->addTab(tab, tr("Erste Mannschaft"));
    }

    // --- Tab 2: youth & second team ---
    {
        auto *tab = new QWidget;
        auto *tabLayout = new QVBoxLayout(tab);
        auto *pitchRow = new QHBoxLayout;
        auto *secondColumn = new QVBoxLayout;
        m_secondXiTitle = new QLabel(tr("Zweitteam-XI"), tab);
        m_secondXiTitle->setObjectName(QStringLiteral("sectionTitle"));
        m_secondXiTitle->setAlignment(Qt::AlignHCenter);
        m_secondXiPitch = new TacticPitchWidget(m_theme, tab);
        secondColumn->addWidget(m_secondXiTitle);
        secondColumn->addWidget(m_secondXiPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
        auto *youthColumn = new QVBoxLayout;
        auto *youthTitle = new QLabel(tr("Jugend-XI"), tab);
        youthTitle->setObjectName(QStringLiteral("sectionTitle"));
        youthTitle->setAlignment(Qt::AlignHCenter);
        m_youthXiPitch = new TacticPitchWidget(m_theme, tab);
        youthColumn->addWidget(youthTitle);
        youthColumn->addWidget(m_youthXiPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
        pitchRow->addLayout(secondColumn, 1);
        pitchRow->addLayout(youthColumn, 1);
        tabLayout->addLayout(pitchRow);

        auto *manageHint = new QLabel(
            tr("✔️ Zum Verwalten dieser Spieler dient die Seite 'Transfers'."), tab);
        tabLayout->addWidget(manageHint);

        const auto makeTable = [tab](const QStringList &headers) {
            auto *table = new QTableWidget(tab);
            table->setColumnCount(headers.size());
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setAlternatingRowColors(true);
            table->setSortingEnabled(true);
            table->setMinimumHeight(240);
            table->horizontalHeader()->setStretchLastSection(true);
            return table;
        };
        m_loanBox = new QGroupBox(tr("Vielversprechende Talente zum Verleihen "
                                     "(nach Talent-Score)"),
                                  tab);
        auto *loanLayout = new QVBoxLayout(m_loanBox);
        m_loanTable = makeTable({tr("Name"), tr("Alter"), tr("Position"), tr("Beste Rolle"),
                                 tr("Bester DWRS"), tr("Talent"), tr("Ent"), tr("Arb"),
                                 tr("Transfer"), tr("Leihe")});
        loanLayout->addWidget(m_loanTable);
        tabLayout->addWidget(m_loanBox);

        m_sellBox = new QGroupBox(tr("Überzählige Spieler für Verkauf / Freigabe"), tab);
        auto *sellLayout = new QVBoxLayout(m_sellBox);
        m_sellTable = makeTable({tr("Name"), tr("Alter"), tr("Position"), tr("Beste Rolle"),
                                 tr("Bester DWRS"), tr("Ent"), tr("Arb"), tr("Transfer"),
                                 tr("Leihe")});
        sellLayout->addWidget(m_sellTable);
        tabLayout->addWidget(m_sellBox);
        tabLayout->addStretch(1);

        m_tabs->addTab(tab, tr("Jugend && Zweitteam"));
    }

    layout->addWidget(m_tabs, 1);

    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuild();
    });

    // Player boxes on the pitches are interactive (M13).
    for (TacticPitchWidget *pitch :
         {m_xiPitch, m_bTeamPitch, m_secondXiPitch, m_youthXiPitch}) {
        connect(pitch, &TacticPitchWidget::playerDoubleClicked, this,
                [this](const QString &uid) { PlayerActions::openProfile(m_context, uid); });
        connect(pitch, &TacticPitchWidget::playerContextMenuRequested, this,
                [this](const QString &uid, const QPoint &globalPos) {
                    PlayerActions::showContextMenu(m_context, this, uid, globalPos);
                });
    }
    PlayerActions::attachToTableWidget(m_context, m_loanTable);
    PlayerActions::attachToTableWidget(m_context, m_sellTable);
    // The depth options carry a player uid in column 1 (Name).
    PlayerActions::attachToTableWidget(m_context, m_depthTable, 1);
}

void BestXiPage::refresh()
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

void BestXiPage::rebuild()
{
    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();
    const QString tactic = m_tacticCombo->currentText();

    if (userClub.isEmpty() || tactic.isEmpty()) {
        m_hint->setText(tr("Bitte wähle zuerst deinen Verein (Dashboard oder Einstellungen)."));
        m_xiPitch->clearData();
        m_bTeamPitch->clearData();
        m_secondXiPitch->clearData();
        m_youthXiPitch->clearData();
        return;
    }

    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    const QStringList slotOrder = m_context.definitions().tacticSlotOrder(tactic);
    const auto layout = m_context.definitions().tacticLayouts().value(tactic);
    const auto roleNames = m_context.definitions().roleDisplayMap();
    const RoleRatings &ratings = m_context.ratings();

    std::vector<const Player *> clubPlayers, secondPlayers;
    for (const Player &player : m_context.store().players()) {
        if (player.club == userClub)
            clubPlayers.push_back(&player);
        else if (!secondClub.isEmpty() && player.club == secondClub)
            secondPlayers.push_back(&player);
    }

    const SquadResult first = m_context.squadBuilder().calculateSquadAndSurplus(
        clubPlayers, positions, slotOrder, ratings);
    const DevelopmentSquads dev = m_context.squadBuilder().calculateDevelopmentSquads(
        secondPlayers, first.depthPool, positions, slotOrder, ratings, first.depthPlayerUids);

    m_xiPitch->setTeam(first.startingXi, positions, layout, roleNames);
    m_bTeamPitch->setTeam(first.bTeam, positions, layout, roleNames);
    m_secondXiTitle->setText(secondClub.isEmpty()
                                 ? tr("Zweitteam-XI (aus Überschuss des Hauptvereins)")
                                 : tr("Zweitteam-XI (%1)").arg(secondClub));
    m_secondXiPitch->setTeam(dev.secondTeamXi, positions, layout, roleNames);
    m_youthXiPitch->setTeam(dev.youthXi, positions, layout, roleNames);

    // --- Depth options grouped by role, naturally sorted ---
    fillDepthTable(first.bestDepthOptions);

    fillSurplusTable(m_loanTable, dev.loanCandidates, true);
    fillSurplusTable(m_sellTable, dev.sellCandidates, false);
    m_loanBox->setVisible(!dev.loanCandidates.empty());
    m_sellBox->setVisible(!dev.sellCandidates.empty());
}

void BestXiPage::fillSurplusTable(QTableWidget *table,
                                  const std::vector<const Player *> &players,
                                  bool includeTalent)
{
    // Port of legacy create_detailed_surplus_df.
    const RoleRatings &ratings = m_context.ratings();
    const auto roleNames = m_context.definitions().roleDisplayMap();
    const int outfielderCap = m_context.config().ageThreshold(QStringLiteral("outfielder"));
    const int goalkeeperCap = m_context.config().ageThreshold(QStringLiteral("goalkeeper"));

    table->setSortingEnabled(false);
    table->clearContents();
    table->setRowCount(static_cast<int>(players.size()));
    int row = 0;
    for (const Player *player : players) {
        double bestDwrs = 0.0;
        QString bestRole;
        for (const QString &role : player->assignedRoles) {
            const double rating = ratings.value(role).value(player->uid, 0.0);
            if (rating > bestDwrs) {
                bestDwrs = rating;
                bestRole = role;
            }
        }

        int column = 0;
        auto *nameItem = new QTableWidgetItem(player->name);
        nameItem->setData(Qt::UserRole, player->uid);
        table->setItem(row, column++, nameItem);
        table->setItem(row, column++,
                       new NumericItem(QString::number(player->age), player->age));
        table->setItem(row, column++, new QTableWidgetItem(player->positionRaw));
        table->setItem(row, column++,
                       new QTableWidgetItem(bestRole.isEmpty()
                                                ? QStringLiteral("–")
                                                : roleNames.value(bestRole, bestRole)));
        auto *dwrsItem = new NumericItem(QStringLiteral("%1%").arg(qRound(bestDwrs)), bestDwrs);
        if (bestDwrs > 0.0) {
            const CellStyle dwrsStyle = dwrsCellStyle(bestDwrs);
            dwrsItem->setBackground(dwrsStyle.background);
            dwrsItem->setForeground(dwrsStyle.text);
        }
        table->setItem(row, column++, dwrsItem);
        if (includeTalent) {
            const double ageCap =
                TalentEngine::ageCapForPlayer(*player, outfielderCap, goalkeeperCap);
            const double talent = TalentEngine::talentForPlayer(m_context.definitions(),
                                                                *player, bestDwrs, ageCap);
            auto *item = new NumericItem(QString::number(qRound(talent)), talent);
            const CellStyle style = dwrsCellStyle(talent);
            if (style.isValid()) {
                item->setBackground(style.background);
                item->setForeground(style.text);
            }
            table->setItem(row, column++, item);
        }
        const auto attrItem = [player](Attr attr) {
            const int value = static_cast<int>(player->attrMean(idx(attr)));
            auto *item = new NumericItem(QString::number(value), value);
            const CellStyle style = attributeCellStyle(value);
            if (style.isValid()) {
                item->setBackground(style.background);
                item->setForeground(style.text);
            }
            return item;
        };
        table->setItem(row, column++, attrItem(Attr::Determination));
        table->setItem(row, column++, attrItem(Attr::WorkRate));
        table->setItem(row, column++,
                       new QTableWidgetItem(player->transferStatus ? QStringLiteral("✅")
                                                                   : QStringLiteral("❌")));
        table->setItem(row, column++,
                       new QTableWidgetItem(player->loanStatus ? QStringLiteral("✅")
                                                               : QStringLiteral("❌")));
        ++row;
    }
    table->setSortingEnabled(true);
    table->resizeColumnsToContents();
}

void BestXiPage::fillDepthTable(const QHash<QString, QList<DepthOption>> &options)
{
    const auto roleNames = m_context.definitions().roleDisplayMap();

    if (options.isEmpty()) {
        m_depthEmpty->setText(
            tr("Keine weiteren Spieler als Tiefen-Optionen für diese Taktik geeignet."));
        m_depthEmpty->setVisible(true);
        m_depthTable->setVisible(false);
        m_depthTable->setRowCount(0);
        return;
    }
    m_depthEmpty->setVisible(false);
    m_depthTable->setVisible(true);

    // One row per (role, player) option, roles in natural pitch order.
    QStringList roles = options.keys();
    roles = m_context.definitions().sortRolesNaturally(roles);
    int rowCount = 0;
    for (const QString &role : roles)
        rowCount += options.value(role).size();

    m_depthTable->setSortingEnabled(false);
    m_depthTable->clearContents();
    m_depthTable->setRowCount(rowCount);
    int row = 0;
    for (const QString &role : roles) {
        for (const DepthOption &option : options.value(role)) {
            int column = 0;
            m_depthTable->setItem(row, column++,
                                  new QTableWidgetItem(roleNames.value(role, role)));

            auto *nameItem = new QTableWidgetItem(option.name);
            nameItem->setData(Qt::UserRole, option.playerUid);
            m_depthTable->setItem(row, column++, nameItem);

            m_depthTable->setItem(row, column++,
                                  new NumericItem(QString::number(option.age), option.age));

            auto *dwrsItem =
                new NumericItem(QStringLiteral("%1%").arg(qRound(option.rating)), option.rating);
            dwrsItem->setTextAlignment(Qt::AlignCenter);
            const CellStyle style = dwrsCellStyle(option.rating);
            if (style.isValid()) {
                dwrsItem->setBackground(style.background);
                dwrsItem->setForeground(style.text);
            }
            m_depthTable->setItem(row, column++, dwrsItem);

            m_depthTable->setItem(row, column++,
                                  new QTableWidgetItem(option.apt.isEmpty()
                                                           ? QStringLiteral("–")
                                                           : option.apt));
            ++row;
        }
    }
    m_depthTable->setSortingEnabled(true);
    m_depthTable->resizeColumnsToContents();
    m_depthTable->horizontalHeader()->setStretchLastSection(true);

    // Size the table to its content so no inner scrollbar and no dead space
    // appears between it and the pitches above.
    m_depthTable->resizeRowsToContents();
    int height = m_depthTable->horizontalHeader()->height() + 2 * m_depthTable->frameWidth();
    for (int r = 0; r < rowCount; ++r)
        height += m_depthTable->rowHeight(r);
    m_depthTable->setFixedHeight(height);
}

} // namespace fm
