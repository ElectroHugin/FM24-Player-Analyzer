#include "TransfersPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "core/Utils.h"

#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

// Column layout of the editable tables.
enum Column {
    ColName = 0,
    ColAge,
    ColBestDwrs,
    ColDet, // youth only
    ColWor, // youth only
};

int transferColumn(bool youth) { return youth ? 5 : 3; }
int loanColumn(bool youth) { return youth ? 6 : 4; }
int newClubColumn(bool youth) { return youth ? 7 : 5; }

} // namespace

TransfersPage::TransfersPage(AppContext &context, QWidget *parent)
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

    auto *heading = new QLabel(
        QStringLiteral("<h2>%1</h2>").arg(tr("Transfer- && Leih-Management")), content);
    layout->addWidget(heading);

    m_hint = new QLabel(
        tr("Verwaltet die überzähligen Spieler für die gewählte Taktik — Spieler, die es "
           "weder in die Erste Elf noch in B-, Zweit- oder Jugendteam geschafft haben. "
           "Häkchen setzen, optional neuen Verein eintragen, dann speichern."),
        content);
    m_hint->setWordWrap(true);
    m_hint->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(m_hint);

    auto *tacticRow = new QHBoxLayout;
    tacticRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_tacticCombo = new QComboBox(content);
    m_tacticCombo->setMinimumWidth(260);
    tacticRow->addWidget(m_tacticCombo);
    tacticRow->addStretch(1);
    layout->addLayout(tacticRow);

    buildSectionUi(&m_loanSection, tr("Zum Verleihen (vielversprechende Talente)"), true,
                   content);
    layout->addWidget(m_loanSection.box);
    buildSectionUi(&m_sellSection, tr("Zum Verkauf / zur Freigabe"), false, content);
    layout->addWidget(m_sellSection.box);
    layout->addStretch(1);

    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuild();
    });
}

void TransfersPage::buildSectionUi(Section *section, const QString &title, bool youth,
                                   QWidget *parent)
{
    section->youth = youth;
    section->baseTitle = title;
    section->box = new QGroupBox(title, parent);
    auto *layout = new QVBoxLayout(section->box);

    section->table = new QTableWidget(section->box);
    QStringList headers{tr("Name"), tr("Alter"), tr("Bester DWRS (Rolle)")};
    if (youth)
        headers << tr("Ent") << tr("Arb");
    headers << tr("Transfer") << tr("Leihe") << tr("Neuer Verein");
    section->table->setColumnCount(headers.size());
    section->table->setHorizontalHeaderLabels(headers);
    section->table->verticalHeader()->setVisible(false);
    section->table->setAlternatingRowColors(true);
    section->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    section->table->horizontalHeader()->setStretchLastSection(true);
    section->table->setMinimumHeight(280);
    // Context menu only: double-click must keep editing the new-club cell.
    PlayerActions::attachToTableWidget(m_context, section->table, ColName, false);
    layout->addWidget(section->table);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    auto *saveButton = new QPushButton(tr("Alle Änderungen dieser Liste speichern"),
                                       section->box);
    buttonRow->addWidget(saveButton);
    layout->addLayout(buttonRow);
    connect(saveButton, &QPushButton::clicked, this,
            [this, section] { saveSection(*section); });
}

void TransfersPage::refresh()
{
    m_updating = true;
    const QString previous = m_tacticCombo->currentText();
    QStringList tactics = m_context.definitions().tacticNames();
    std::sort(tactics.begin(), tactics.end());
    m_tacticCombo->clear();
    m_tacticCombo->addItems(tactics);
    QString wanted = previous;
    if (wanted.isEmpty())
        wanted = m_context.database().setting(QStringLiteral("favorite_tactic_1"));
    if (!wanted.isEmpty() && tactics.contains(wanted))
        m_tacticCombo->setCurrentText(wanted);
    m_updating = false;
    rebuild();
}

void TransfersPage::rebuild()
{
    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();
    const QString tactic = m_tacticCombo->currentText();

    if (userClub.isEmpty() || tactic.isEmpty()) {
        m_loanSection.box->setVisible(false);
        m_sellSection.box->setVisible(false);
        m_hint->setText(tr("Bitte wähle zuerst deinen Verein (Dashboard oder Einstellungen)."));
        return;
    }
    m_loanSection.box->setVisible(true);
    m_sellSection.box->setVisible(true);

    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    const QStringList slotOrder = m_context.definitions().tacticSlotOrder(tactic);
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
        secondPlayers, first.depthPool, positions, slotOrder, ratings,
        first.depthPlayerUids);

    fillTable(m_loanSection, dev.loanCandidates);
    fillTable(m_sellSection, dev.sellCandidates);
}

void TransfersPage::fillTable(Section &section, const std::vector<const Player *> &players)
{
    const RoleRatings &ratings = m_context.ratings();
    QTableWidget *table = section.table;
    const bool youth = section.youth;

    table->clearContents();
    table->setRowCount(static_cast<int>(players.size()));

    int row = 0;
    for (const Player *player : players) {
        // Best assigned role by normalized DWRS.
        double bestDwrs = 0.0;
        QString bestRole;
        for (const QString &role : player->assignedRoles) {
            const double rating = ratings.value(role).value(player->uid, 0.0);
            if (rating > bestDwrs) {
                bestDwrs = rating;
                bestRole = role;
            }
        }

        auto *nameItem = new QTableWidgetItem(player->name);
        nameItem->setData(Qt::UserRole, player->uid);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, ColName, nameItem);

        auto *ageItem = new QTableWidgetItem(QString::number(player->age));
        ageItem->setFlags(ageItem->flags() & ~Qt::ItemIsEditable);
        if (youth && player->age <= 17)
            ageItem->setForeground(QColor(0xf5, 0x85, 0x85));
        table->setItem(row, ColAge, ageItem);

        auto *dwrsItem = new QTableWidgetItem(
            bestRole.isEmpty() ? QStringLiteral("–")
                               : QStringLiteral("%1% (%2)").arg(qRound(bestDwrs)).arg(bestRole));
        dwrsItem->setFlags(dwrsItem->flags() & ~Qt::ItemIsEditable);
        if (bestDwrs > 0.0) {
            const CellStyle dwrsStyle = dwrsCellStyle(bestDwrs);
            dwrsItem->setBackground(dwrsStyle.background);
            dwrsItem->setForeground(dwrsStyle.text);
        }
        table->setItem(row, ColBestDwrs, dwrsItem);

        if (youth) {
            const auto attrItem = [&](Attr attr) {
                const int value = static_cast<int>(player->attrMean(idx(attr)));
                auto *item = new QTableWidgetItem(QString::number(value));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                const CellStyle style = attributeCellStyle(value);
                if (style.isValid()) {
                    item->setBackground(style.background);
                    item->setForeground(style.text);
                }
                return item;
            };
            table->setItem(row, ColDet, attrItem(Attr::Determination));
            table->setItem(row, ColWor, attrItem(Attr::WorkRate));
        }

        const auto checkItem = [](bool checked) {
            auto *item = new QTableWidgetItem;
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            return item;
        };
        table->setItem(row, transferColumn(youth), checkItem(player->transferStatus));
        table->setItem(row, loanColumn(youth), checkItem(player->loanStatus));

        auto *clubItem = new QTableWidgetItem(player->newClub);
        table->setItem(row, newClubColumn(youth), clubItem);
        ++row;
    }
    table->resizeColumnsToContents();
    section.box->setTitle(tr("%1 (%2)").arg(section.baseTitle).arg(players.size()));
}

void TransfersPage::saveSection(const Section &section)
{
    const bool youth = section.youth;
    std::vector<Player> batch;

    for (int row = 0; row < section.table->rowCount(); ++row) {
        const QTableWidgetItem *nameItem = section.table->item(row, ColName);
        if (!nameItem)
            continue;
        const QString uid = nameItem->data(Qt::UserRole).toString();
        const int storeRow = m_context.store().rowByUid(uid);
        if (storeRow < 0)
            continue;

        Player &player = m_context.store().at(storeRow);
        const bool transfer =
            section.table->item(row, transferColumn(youth))->checkState() == Qt::Checked;
        const bool loan =
            section.table->item(row, loanColumn(youth))->checkState() == Qt::Checked;
        const QString newClub =
            section.table->item(row, newClubColumn(youth))->text().trimmed();

        // Legacy behavior: a non-empty "new club" moves the player immediately.
        const bool clubChanges = !newClub.isEmpty() && newClub != player.club;
        if (transfer == player.transferStatus && loan == player.loanStatus && !clubChanges
            && newClub == player.newClub)
            continue;

        player.transferStatus = transfer;
        player.loanStatus = loan;
        player.newClub = newClub;
        if (clubChanges)
            player.club = newClub;
        batch.push_back(player);
    }

    if (batch.empty()) {
        QMessageBox::information(this, tr("Transfers"), tr("Keine Änderungen zum Speichern."));
        return;
    }
    if (!m_context.database().upsertPlayers(batch)) {
        QMessageBox::critical(this, tr("Transfers"), m_context.database().errorString());
        return;
    }
    const int count = static_cast<int>(batch.size());
    m_context.reloadFromDatabase();
    QMessageBox::information(this, tr("Transfers"),
                             tr("%1 Spieler-Datensätze gespeichert.").arg(count));
}

} // namespace fm
