#include "BestXiPage.h"

#include "../AppContext.h"
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
        xiColumn->addStretch(1);
        auto *bColumn = new QVBoxLayout;
        auto *bTitle = new QLabel(tr("B-Team"), tab);
        bTitle->setObjectName(QStringLiteral("sectionTitle"));
        bTitle->setAlignment(Qt::AlignHCenter);
        m_bTeamPitch = new TacticPitchWidget(m_theme, tab);
        bColumn->addWidget(bTitle);
        bColumn->addWidget(m_bTeamPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
        bColumn->addStretch(1);
        pitchRow->addLayout(xiColumn, 1);
        pitchRow->addLayout(bColumn, 1);
        tabLayout->addLayout(pitchRow);

        auto *depthBox = new QGroupBox(tr("Weitere Kader-Tiefe"), tab);
        auto *depthLayout = new QVBoxLayout(depthBox);
        m_depthLabel = new QLabel(depthBox);
        m_depthLabel->setWordWrap(true);
        m_depthLabel->setTextFormat(Qt::RichText);
        depthLayout->addWidget(m_depthLabel);
        tabLayout->addWidget(depthBox);
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
        secondColumn->addStretch(1);
        auto *youthColumn = new QVBoxLayout;
        auto *youthTitle = new QLabel(tr("Jugend-XI"), tab);
        youthTitle->setObjectName(QStringLiteral("sectionTitle"));
        youthTitle->setAlignment(Qt::AlignHCenter);
        m_youthXiPitch = new TacticPitchWidget(m_theme, tab);
        youthColumn->addWidget(youthTitle);
        youthColumn->addWidget(m_youthXiPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
        youthColumn->addStretch(1);
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

        m_tabs->addTab(tab, tr("Jugend && Zweitteam"));
    }

    layout->addWidget(m_tabs, 1);

    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuild();
    });
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
    if (first.bestDepthOptions.isEmpty()) {
        m_depthLabel->setText(
            tr("Keine weiteren Spieler als Tiefen-Optionen für diese Taktik geeignet."));
    } else {
        QStringList roles = first.bestDepthOptions.keys();
        roles = m_context.definitions().sortRolesNaturally(roles);
        QString html;
        for (const QString &role : roles) {
            const auto options = first.bestDepthOptions.value(role);
            if (options.isEmpty())
                continue;
            QStringList entries;
            for (const DepthOption &option : options) {
                QString entry = tr("%1 (%2) – %3%")
                                    .arg(option.name.toHtmlEscaped())
                                    .arg(option.age)
                                    .arg(qRound(option.rating));
                if (!option.apt.isEmpty())
                    entry += QStringLiteral(" – %1").arg(option.apt.toHtmlEscaped());
                entries << entry;
            }
            html += QStringLiteral("<p><b>%1</b>: %2</p>")
                        .arg(roleNames.value(role, role).toHtmlEscaped(),
                             entries.join(QStringLiteral(", ")));
        }
        m_depthLabel->setText(html);
    }

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
        table->setItem(row, column++, new QTableWidgetItem(player->name));
        table->setItem(row, column++,
                       new NumericItem(QString::number(player->age), player->age));
        table->setItem(row, column++, new QTableWidgetItem(player->positionRaw));
        table->setItem(row, column++,
                       new QTableWidgetItem(bestRole.isEmpty()
                                                ? QStringLiteral("–")
                                                : roleNames.value(bestRole, bestRole)));
        table->setItem(row, column++,
                       new NumericItem(QStringLiteral("%1%").arg(qRound(bestDwrs)), bestDwrs));
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

} // namespace fm
