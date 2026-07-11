#include "AssignRolesPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "../RecalcHelper.h"
#include "../widgets/PlayerTableModel.h"
#include "core/Utils.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

enum FilterMode {
    AllPlayers = 0,
    Unassigned,
    NotFromMyClub,
    UnassignedNotFromMyClub,
};

} // namespace

AssignRolesPage::AssignRolesPage(AppContext &context, QWidget *parent)
    : PageBase(context, parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Rollen zuweisen")), this);
    layout->addWidget(heading);

    // --- Filter row ---
    auto *filterRow = new QHBoxLayout;
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(tr("Alle Spieler"), AllPlayers);
    m_filterCombo->addItem(tr("Spieler ohne Rollen"), Unassigned);
    m_filterCombo->addItem(tr("Nicht von meinem Verein"), NotFromMyClub);
    m_filterCombo->addItem(tr("Ohne Rollen, nicht von meinem Verein"), UnassignedNotFromMyClub);
    m_filterCombo->setCurrentIndex(1); // legacy default: Unassigned Players
    m_clubCombo = new QComboBox(this);
    m_clubCombo->setMinimumWidth(180);
    m_positionCombo = new QComboBox(this);
    m_positionCombo->setMinimumWidth(140);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Nach Name suchen…"));
    m_searchEdit->setClearButtonEnabled(true);
    filterRow->addWidget(new QLabel(tr("Filter:"), this));
    filterRow->addWidget(m_filterCombo);
    filterRow->addWidget(new QLabel(tr("Verein:"), this));
    filterRow->addWidget(m_clubCombo);
    filterRow->addWidget(new QLabel(tr("Position:"), this));
    filterRow->addWidget(m_positionCombo);
    filterRow->addWidget(m_searchEdit, 1);
    layout->addLayout(filterRow);

    // --- Auto-assign row ---
    auto *autoRow = new QHBoxLayout;
    auto *autoUnassigned = new QPushButton(tr("Auto-Zuweisung (nur Spieler ohne Rollen)"), this);
    auto *autoAll = new QPushButton(tr("⚠️ Auto-Zuweisung (ALLE Spieler)"), this);
    m_pendingLabel = new QLabel(this);
    m_saveButton = new QPushButton(tr("Änderungen speichern"), this);
    m_saveButton->setEnabled(false);
    m_saveButton->setDefault(true);
    autoRow->addWidget(autoUnassigned);
    autoRow->addWidget(autoAll);
    autoRow->addStretch(1);
    autoRow->addWidget(m_pendingLabel);
    autoRow->addWidget(m_saveButton);
    layout->addLayout(autoRow);
    connect(autoUnassigned, &QPushButton::clicked, this, [this] { autoAssign(false); });
    connect(autoAll, &QPushButton::clicked, this, [this] { autoAssign(true); });
    connect(m_saveButton, &QPushButton::clicked, this, &AssignRolesPage::savePending);

    // --- Table + role editor ---
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_model = new PlayerTableModel(this);
    m_model->setColumns({
        {tr("Name"), [](const Player &p) { return p.name; },
         [](const Player &p) { return getLastName(p.name); }, nullptr, {}},
        {tr("Alter"), [](const Player &p) { return p.age; }, nullptr, nullptr,
         Qt::AlignRight | Qt::AlignVCenter},
        {tr("Position"), [](const Player &p) { return p.positionRaw; }, nullptr, nullptr, {}},
        {tr("Verein"), [](const Player &p) { return p.club; }, nullptr, nullptr, {}},
        {tr("Zugewiesene Rollen"),
         [](const Player &p) { return p.assignedRoles.join(QStringLiteral(", ")); }, nullptr,
         nullptr, {}},
    });
    m_proxy = new PlayerFilterProxy(m_model, this);
    m_table = new QTableView(splitter);
    m_table->setModel(m_proxy);
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(0, Qt::AscendingOrder);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    PlayerActions::attachToView(m_context, m_table, m_proxy);
    splitter->addWidget(m_table);

    auto *editorPanel = new QWidget(splitter);
    auto *editorLayout = new QVBoxLayout(editorPanel);
    editorLayout->setContentsMargins(8, 0, 0, 0);
    m_editorTitle = new QLabel(tr("Spieler in der Tabelle auswählen, um Rollen zu bearbeiten."),
                               editorPanel);
    m_editorTitle->setWordWrap(true);
    m_roleList = new QListWidget(editorPanel);
    m_roleList->setEnabled(false);
    editorLayout->addWidget(m_editorTitle);
    editorLayout->addWidget(m_roleList, 1);
    splitter->addWidget(editorPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updatingFilters)
            applyFilters();
    });
    connect(m_clubCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updatingFilters)
            applyFilters();
    });
    connect(m_positionCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updatingFilters)
            applyFilters();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { m_proxy->setNameFilter(text); });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
                showEditorFor(current.isValid() ? m_proxy->playerAt(current.row()) : nullptr);
            });
    connect(m_roleList, &QListWidget::itemChanged, this,
            &AssignRolesPage::editorSelectionChanged);
}

void AssignRolesPage::refresh()
{
    m_pending.clear();
    m_saveButton->setEnabled(false);
    m_pendingLabel->clear();
    rebuildFilters();
    applyFilters();
    showEditorFor(nullptr);
}

void AssignRolesPage::rebuildFilters()
{
    m_updatingFilters = true;
    const QString club = m_clubCombo->currentText();
    const QString position = m_positionCombo->currentText();

    QSet<QString> clubs, positions;
    for (const Player &player : m_context.store().players()) {
        if (!player.club.isEmpty())
            clubs.insert(player.club);
        if (!player.positionRaw.isEmpty())
            positions.insert(player.positionRaw);
    }
    QStringList clubList(clubs.cbegin(), clubs.cend());
    QStringList positionList(positions.cbegin(), positions.cend());
    std::sort(clubList.begin(), clubList.end());
    std::sort(positionList.begin(), positionList.end());

    m_clubCombo->clear();
    m_clubCombo->addItem(tr("Alle"));
    m_clubCombo->addItems(clubList);
    if (!club.isEmpty())
        m_clubCombo->setCurrentText(club);
    m_positionCombo->clear();
    m_positionCombo->addItem(tr("Alle"));
    m_positionCombo->addItems(positionList);
    if (!position.isEmpty())
        m_positionCombo->setCurrentText(position);
    m_updatingFilters = false;
}

void AssignRolesPage::applyFilters()
{
    const int mode = m_filterCombo->currentData().toInt();
    const QString userClub = m_context.userClub();
    const bool clubAll = m_clubCombo->currentIndex() <= 0;
    const QString club = m_clubCombo->currentText();
    const bool posAll = m_positionCombo->currentIndex() <= 0;
    const QString position = m_positionCombo->currentText();

    std::vector<const Player *> rows;
    for (const Player &player : m_context.store().players()) {
        switch (mode) {
        case Unassigned:
            if (!player.assignedRoles.isEmpty())
                continue;
            break;
        case NotFromMyClub:
            if (!userClub.isEmpty() && player.club == userClub)
                continue;
            break;
        case UnassignedNotFromMyClub:
            if (!player.assignedRoles.isEmpty()
                || (!userClub.isEmpty() && player.club == userClub))
                continue;
            break;
        default:
            break;
        }
        if (!clubAll && player.club != club)
            continue;
        if (!posAll && player.positionRaw != position)
            continue;
        rows.push_back(&player);
    }
    m_model->setRows(std::move(rows));
}

void AssignRolesPage::showEditorFor(const Player *player)
{
    m_updatingEditor = true;
    m_roleList->clear();

    if (!player) {
        m_editorUid.clear();
        m_editorTitle->setText(tr("Spieler in der Tabelle auswählen, um Rollen zu bearbeiten."));
        m_roleList->setEnabled(false);
        m_updatingEditor = false;
        return;
    }

    m_editorUid = player->uid;
    m_editorTitle->setText(
        QStringLiteral("<b>%1</b> (%2)").arg(player->name.toHtmlEscaped(),
                                             player->positionRaw.toHtmlEscaped()));
    m_roleList->setEnabled(true);

    const QStringList currentRoles = m_pending.value(player->uid, player->assignedRoles);
    const auto roleNames = m_context.definitions().roleDisplayMap();
    const auto categories = m_context.definitions().playerRoles();

    for (const QString &category : Definitions::roleCategoryOrder()) {
        const auto rolesInCategory = categories.value(category);
        if (rolesInCategory.isEmpty())
            continue;
        auto *header = new QListWidgetItem(category, m_roleList);
        header->setFlags(Qt::NoItemFlags);
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);

        QStringList abbrs = rolesInCategory.keys();
        abbrs = m_context.definitions().sortRolesNaturally(abbrs);
        for (const QString &abbr : abbrs) {
            auto *item = new QListWidgetItem(roleNames.value(abbr, abbr), m_roleList);
            item->setData(Qt::UserRole, abbr);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(currentRoles.contains(abbr) ? Qt::Checked : Qt::Unchecked);
        }
    }
    m_updatingEditor = false;
}

QStringList AssignRolesPage::editorRoles() const
{
    QStringList roles;
    for (int i = 0; i < m_roleList->count(); ++i) {
        const QListWidgetItem *item = m_roleList->item(i);
        if (item->checkState() == Qt::Checked)
            roles << item->data(Qt::UserRole).toString();
    }
    std::sort(roles.begin(), roles.end());
    return roles;
}

void AssignRolesPage::editorSelectionChanged()
{
    if (m_updatingEditor || m_editorUid.isEmpty())
        return;
    stagePendingFromEditor();
}

void AssignRolesPage::stagePendingFromEditor()
{
    const Player *player = m_context.store().findByUid(m_editorUid);
    if (!player)
        return;

    QStringList newRoles = editorRoles();
    QStringList oldRoles = player->assignedRoles;
    std::sort(oldRoles.begin(), oldRoles.end());
    if (newRoles == oldRoles)
        m_pending.remove(m_editorUid);
    else
        m_pending.insert(m_editorUid, newRoles);

    m_saveButton->setEnabled(!m_pending.isEmpty());
    m_pendingLabel->setText(
        m_pending.isEmpty() ? QString()
                            : tr("%1 Spieler mit ungespeicherten Änderungen").arg(m_pending.size()));
}

void AssignRolesPage::savePending()
{
    if (m_pending.isEmpty()) {
        QMessageBox::information(this, tr("Rollen"), tr("Keine Änderungen zum Speichern."));
        return;
    }

    std::vector<Player> batch;
    QStringList affectedUids;
    for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
        const int row = m_context.store().rowByUid(it.key());
        if (row < 0)
            continue;
        Player &player = m_context.store().at(row);
        player.assignedRoles = it.value();
        batch.push_back(player);
        affectedUids << it.key();
    }
    if (!m_context.database().upsertPlayers(batch)) {
        QMessageBox::critical(this, tr("Rollen"), m_context.database().errorString());
        return;
    }
    const int count = static_cast<int>(batch.size());
    m_pending.clear();

    recalcDwrsFor(m_context, this, affectedUids, [this, count](const QString &error) {
        if (!error.isEmpty())
            QMessageBox::critical(this, tr("Rollen"), error);
        else
            QMessageBox::information(
                this, tr("Rollen"),
                tr("Rollen für %1 Spieler aktualisiert und DWRS neu berechnet.").arg(count));
    });
}

void AssignRolesPage::autoAssign(bool allPlayers)
{
    if (allPlayers
        && QMessageBox::question(
               this, tr("Auto-Zuweisung"),
               tr("Wirklich die Rollen ALLER Spieler anhand ihrer Positionen neu setzen? "
                  "Manuell angepasste Rollen gehen dabei verloren."))
               != QMessageBox::Yes) {
        return;
    }

    const auto posMap = m_context.definitions().positionToRoleMapping();
    QHash<QString, QStringList> rolesForPosition;
    const auto defaultRolesFor = [&](const Player &player) {
        auto it = rolesForPosition.find(player.positionRaw);
        if (it == rolesForPosition.end()) {
            QSet<QString> roles;
            for (const QString &position : parsePositionString(player.positionRaw)) {
                for (const QString &role : posMap.value(position))
                    roles.insert(role);
            }
            QStringList sorted(roles.cbegin(), roles.cend());
            std::sort(sorted.begin(), sorted.end());
            it = rolesForPosition.insert(player.positionRaw, sorted);
        }
        return it.value();
    };

    std::vector<Player> batch;
    QStringList affectedUids;
    for (const Player &player : m_context.store().players()) {
        if (!allPlayers && !player.assignedRoles.isEmpty())
            continue;
        const QStringList roles = defaultRolesFor(player);
        if (roles.isEmpty())
            continue;
        QStringList oldRoles = player.assignedRoles;
        std::sort(oldRoles.begin(), oldRoles.end());
        if (roles == oldRoles)
            continue;
        const int row = m_context.store().rowByUid(player.uid);
        Player &mutablePlayer = m_context.store().at(row);
        mutablePlayer.assignedRoles = roles;
        batch.push_back(mutablePlayer);
        affectedUids << player.uid;
    }

    if (batch.empty()) {
        QMessageBox::information(this, tr("Auto-Zuweisung"),
                                 tr("Keine Spieler zu aktualisieren."));
        return;
    }
    if (!m_context.database().upsertPlayers(batch)) {
        QMessageBox::critical(this, tr("Auto-Zuweisung"), m_context.database().errorString());
        return;
    }
    const int count = static_cast<int>(batch.size());
    recalcDwrsFor(m_context, this, affectedUids, [this, count](const QString &error) {
        if (!error.isEmpty())
            QMessageBox::critical(this, tr("Auto-Zuweisung"), error);
        else
            QMessageBox::information(
                this, tr("Auto-Zuweisung"),
                tr("%1 Spielern Rollen zugewiesen und DWRS neu berechnet.").arg(count));
    });
}

} // namespace fm
