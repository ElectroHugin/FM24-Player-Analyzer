#include "EditPlayerPage.h"

#include "../AppContext.h"
#include "core/Constants.h"
#include "core/Utils.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

EditPlayerPage::EditPlayerPage(AppContext &context, QWidget *parent)
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
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Spieler bearbeiten")), content);
    layout->addWidget(heading);

    // --- Selection: club players (with completeness markers) or global search.
    auto *selectRow = new QHBoxLayout;
    auto *clubColumn = new QVBoxLayout;
    auto *clubTitle = new QLabel(tr("Spieler meines Vereins"), content);
    clubTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto *markerHint = new QLabel(
        tr("Markierungen für fehlende Daten: 🎯 Primärrolle, 📄 Spielzeit, 📍 natürliche Positionen"),
        content);
    markerHint->setObjectName(QStringLiteral("kpiCaption"));
    m_clubPlayerCombo = new QComboBox(content);
    m_clubPlayerCombo->setMinimumWidth(320);
    clubColumn->addWidget(clubTitle);
    clubColumn->addWidget(markerHint);
    clubColumn->addWidget(m_clubPlayerCombo);
    selectRow->addLayout(clubColumn, 1);

    auto *searchColumn = new QVBoxLayout;
    auto *searchTitle = new QLabel(tr("Oder: alle Spieler durchsuchen"), content);
    searchTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_searchEdit = new QLineEdit(content);
    m_searchEdit->setPlaceholderText(tr("Nach Name suchen…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchResultCombo = new QComboBox(content);
    m_searchResultCombo->setMinimumWidth(320);
    searchColumn->addWidget(searchTitle);
    searchColumn->addWidget(m_searchEdit);
    searchColumn->addWidget(m_searchResultCombo);
    selectRow->addLayout(searchColumn, 1);
    layout->addLayout(selectRow);

    auto *divider = new QFrame(content);
    divider->setFrameShape(QFrame::HLine);
    layout->addWidget(divider);

    // --- Editor ---
    m_editorBox = new QGroupBox(content);
    auto *editorLayout = new QHBoxLayout(m_editorBox);

    auto *adminColumn = new QVBoxLayout;
    auto *adminTitle = new QLabel(tr("Verwaltung"), m_editorBox);
    adminTitle->setObjectName(QStringLiteral("sectionTitle"));
    adminColumn->addWidget(adminTitle);
    auto *adminForm = new QFormLayout;
    m_clubEdit = new QLineEdit(m_editorBox);
    adminForm->addRow(tr("Verein:"), m_clubEdit);
    m_aptCombo = new QComboBox(m_editorBox);
    m_aptLabel = new QLabel(tr("Spielzeit (Agreed Playing Time):"), m_editorBox);
    adminForm->addRow(m_aptLabel, m_aptCombo);
    adminColumn->addLayout(adminForm);
    adminColumn->addStretch(1);
    editorLayout->addLayout(adminColumn, 1);

    m_tacticalColumn = new QWidget(m_editorBox);
    auto *tacticalLayout = new QVBoxLayout(m_tacticalColumn);
    tacticalLayout->setContentsMargins(0, 0, 0, 0);
    auto *tacticalTitle = new QLabel(tr("Taktisches Profil"), m_tacticalColumn);
    tacticalTitle->setObjectName(QStringLiteral("sectionTitle"));
    tacticalLayout->addWidget(tacticalTitle);
    tacticalLayout->addWidget(new QLabel(tr("Natürliche Positionen:"), m_tacticalColumn));
    m_naturalPositionsList = new QListWidget(m_tacticalColumn);
    m_naturalPositionsList->setMaximumHeight(120);
    m_naturalPositionsList->setToolTip(
        tr("Die Positionen, auf denen dieser Spieler am effektivsten ist "
           "(Bonus in der Kader-Berechnung)."));
    tacticalLayout->addWidget(m_naturalPositionsList);
    auto *tacticalForm = new QFormLayout;
    m_primaryRoleCombo = new QComboBox(m_tacticalColumn);
    tacticalForm->addRow(tr("Primärrolle:"), m_primaryRoleCombo);
    m_preferredSideCombo = new QComboBox(m_tacticalColumn);
    m_preferredSideCombo->addItem(tr("Keine"), QString());
    m_preferredSideCombo->addItem(tr("Links"), QStringLiteral("Left"));
    m_preferredSideCombo->addItem(tr("Rechts"), QStringLiteral("Right"));
    m_preferredSideCombo->setToolTip(
        tr("Überschreibt den bevorzugten Fuß im Best-XI-Rechner bei symmetrischen Rollen."));
    tacticalForm->addRow(tr("Bevorzugte Seite:"), m_preferredSideCombo);
    tacticalLayout->addLayout(tacticalForm);
    tacticalLayout->addStretch(1);
    editorLayout->addWidget(m_tacticalColumn, 1);

    layout->addWidget(m_editorBox);

    auto *saveRow = new QHBoxLayout;
    saveRow->addStretch(1);
    m_saveButton = new QPushButton(tr("Änderungen speichern"), content);
    m_saveButton->setDefault(true);
    saveRow->addWidget(m_saveButton);
    layout->addLayout(saveRow);
    layout->addStretch(1);

    connect(m_clubPlayerCombo, &QComboBox::activated, this, [this] {
        m_currentUid = m_clubPlayerCombo->currentData().toString();
        showEditor();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &EditPlayerPage::runSearch);
    connect(m_searchResultCombo, &QComboBox::activated, this, [this] {
        m_currentUid = m_searchResultCombo->currentData().toString();
        showEditor();
    });
    connect(m_saveButton, &QPushButton::clicked, this, &EditPlayerPage::save);
}

const Player *EditPlayerPage::currentPlayer() const
{
    return m_context.store().findByUid(m_currentUid);
}

void EditPlayerPage::refresh()
{
    rebuildClubCombo();
    runSearch();
    showEditor();
}

void EditPlayerPage::rebuildClubCombo()
{
    m_updating = true;
    m_clubPlayerCombo->clear();
    m_clubPlayerCombo->addItem(tr("— Spieler auswählen —"), QString());

    const QString userClub = m_context.userClub();
    std::vector<const Player *> players;
    for (const Player &player : m_context.store().players()) {
        if (!userClub.isEmpty() && player.club == userClub)
            players.push_back(&player);
    }
    std::sort(players.begin(), players.end(), [](const Player *a, const Player *b) {
        return getLastName(a->name).localeAwareCompare(getLastName(b->name)) < 0;
    });
    for (const Player *player : players) {
        QStringList markers;
        if (player->primaryRole.isEmpty())
            markers << QStringLiteral("🎯");
        if (player->agreedPlayingTime.isEmpty())
            markers << QStringLiteral("📄");
        if (player->naturalPositions.isEmpty())
            markers << QStringLiteral("📍");
        const QString label = markers.isEmpty()
                                  ? player->name
                                  : QStringLiteral("%1  %2").arg(player->name,
                                                                 markers.join(QLatin1Char(' ')));
        m_clubPlayerCombo->addItem(label, player->uid);
    }
    const int index = m_clubPlayerCombo->findData(m_currentUid);
    if (index > 0)
        m_clubPlayerCombo->setCurrentIndex(index);
    m_updating = false;
}

void EditPlayerPage::runSearch()
{
    m_searchResultCombo->clear();
    const QString query = m_searchEdit->text().trimmed();
    if (query.length() < 2)
        return;
    int count = 0;
    for (const Player &player : m_context.store().players()) {
        if (!player.name.contains(query, Qt::CaseInsensitive))
            continue;
        m_searchResultCombo->addItem(
            QStringLiteral("%1 (%2)").arg(player.name, player.club), player.uid);
        if (++count >= 50)
            break;
    }
}

void EditPlayerPage::showEditor()
{
    const Player *player = currentPlayer();
    m_editorBox->setVisible(player != nullptr);
    m_saveButton->setVisible(player != nullptr);
    if (!player)
        return;

    m_updating = true;
    m_editorBox->setTitle(tr("Bearbeite: %1 (%2)").arg(player->name, player->club));
    m_clubEdit->setText(player->club);

    const bool isClubPlayer = !m_context.userClub().isEmpty()
                              && player->club == m_context.userClub();

    // APT options depend on GK vs field player (legacy).
    const bool isGk = player->positionRaw.contains(QLatin1String("GK"));
    const QStringList aptOptions = isGk ? gkAptOptions() : fieldPlayerAptOptions();
    m_aptCombo->clear();
    for (const QString &option : aptOptions)
        m_aptCombo->addItem(option == QLatin1String("None") ? tr("Keine") : option,
                            option == QLatin1String("None") ? QString() : option);
    const int aptIndex = m_aptCombo->findData(player->agreedPlayingTime);
    m_aptCombo->setCurrentIndex(aptIndex >= 0 ? aptIndex : 0);
    m_aptCombo->setVisible(isClubPlayer);
    m_aptLabel->setVisible(isClubPlayer);

    // Tactical profile only for own club players (legacy).
    m_tacticalColumn->setVisible(isClubPlayer);
    if (isClubPlayer) {
        m_naturalPositionsList->clear();
        const QSet<QString> parsed = player->parsedPositions();
        QStringList positions(parsed.cbegin(), parsed.cend());
        std::sort(positions.begin(), positions.end());
        for (const QString &position : positions) {
            auto *item = new QListWidgetItem(position, m_naturalPositionsList);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(player->naturalPositions.contains(position) ? Qt::Checked
                                                                            : Qt::Unchecked);
        }

        const auto roleNames = m_context.definitions().roleDisplayMap();
        m_primaryRoleCombo->clear();
        m_primaryRoleCombo->addItem(tr("Keine"), QString());
        QStringList roles = player->assignedRoles;
        std::sort(roles.begin(), roles.end());
        for (const QString &role : roles)
            m_primaryRoleCombo->addItem(roleNames.value(role, role), role);
        const int roleIndex = m_primaryRoleCombo->findData(player->primaryRole);
        m_primaryRoleCombo->setCurrentIndex(roleIndex >= 0 ? roleIndex : 0);

        const int sideIndex = m_preferredSideCombo->findData(player->preferredSide);
        m_preferredSideCombo->setCurrentIndex(sideIndex >= 0 ? sideIndex : 0);
    }
    m_updating = false;
}

void EditPlayerPage::save()
{
    const Player *player = currentPlayer();
    if (!player)
        return;
    const int row = m_context.store().rowByUid(player->uid);
    if (row < 0)
        return;

    const bool isClubPlayer = !m_context.userClub().isEmpty()
                              && player->club == m_context.userClub();

    Player &mutablePlayer = m_context.store().at(row);
    const QString newClub = m_clubEdit->text().trimmed();
    if (!newClub.isEmpty())
        mutablePlayer.club = newClub;
    if (isClubPlayer) {
        mutablePlayer.agreedPlayingTime = m_aptCombo->currentData().toString();
        QStringList naturalPositions;
        for (int i = 0; i < m_naturalPositionsList->count(); ++i) {
            const QListWidgetItem *item = m_naturalPositionsList->item(i);
            if (item->checkState() == Qt::Checked)
                naturalPositions << item->text();
        }
        mutablePlayer.naturalPositions = naturalPositions;
        mutablePlayer.primaryRole = m_primaryRoleCombo->currentData().toString();
        mutablePlayer.preferredSide = m_preferredSideCombo->currentData().toString();
    }

    std::vector<Player> batch{mutablePlayer};
    if (!m_context.database().upsertPlayers(batch)) {
        QMessageBox::critical(this, tr("Spieler bearbeiten"),
                              m_context.database().errorString());
        return;
    }
    const QString name = mutablePlayer.name;
    m_context.reloadFromDatabase();
    QMessageBox::information(this, tr("Spieler bearbeiten"),
                             tr("Änderungen für %1 gespeichert.").arg(name));
}

} // namespace fm
