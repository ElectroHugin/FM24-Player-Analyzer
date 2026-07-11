#include "NationalSquadSelectionPage.h"

#include "../AppContext.h"
#include "PageHelpers.h"
#include "core/Utils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace fm {

namespace {

QListWidgetItem *playerItem(const Player &player, QListWidget *list)
{
    auto *item = new QListWidgetItem(
        QStringLiteral("%1 (%2)\n%3 | %4")
            .arg(player.name)
            .arg(player.age)
            .arg(player.club, player.positionRaw),
        list);
    item->setData(Qt::UserRole, player.uid);
    return item;
}

} // namespace

NationalSquadSelectionPage::NationalSquadSelectionPage(AppContext &context, QWidget *parent)
    : PageBase(context, parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    auto *heading =
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Nationalkader-Auswahl")), this);
    layout->addWidget(heading);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    auto *columnsRow = new QHBoxLayout;

    // --- Left: available pool ---
    auto *leftColumn = new QVBoxLayout;
    auto *availableTitle = new QLabel(tr("Verfügbarer Spieler-Pool"), this);
    availableTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Verfügbare Spieler nach Name durchsuchen…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_availableList = new QListWidget(this);
    m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    leftColumn->addWidget(availableTitle);
    leftColumn->addWidget(m_searchEdit);
    leftColumn->addWidget(m_availableList, 1);
    columnsRow->addLayout(leftColumn, 1);

    // --- Middle: move buttons ---
    auto *buttonColumn = new QVBoxLayout;
    buttonColumn->addStretch(1);
    auto *addButton = new QPushButton(tr("Hinzufügen →"), this);
    auto *removeButton = new QPushButton(tr("← Entfernen"), this);
    buttonColumn->addWidget(addButton);
    buttonColumn->addWidget(removeButton);
    buttonColumn->addStretch(1);
    columnsRow->addLayout(buttonColumn);

    // --- Right: current squad ---
    auto *rightColumn = new QVBoxLayout;
    m_squadTitle = new QLabel(this);
    m_squadTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_squadList = new QListWidget(this);
    m_squadList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rightColumn->addWidget(m_squadTitle);
    rightColumn->addWidget(m_squadList, 1);
    columnsRow->addLayout(rightColumn, 1);

    layout->addLayout(columnsRow, 1);

    auto *saveRow = new QHBoxLayout;
    saveRow->addStretch(1);
    m_saveButton = new QPushButton(tr("Nationalkader speichern"), this);
    m_saveButton->setDefault(true);
    saveRow->addWidget(m_saveButton);
    layout->addLayout(saveRow);

    connect(m_searchEdit, &QLineEdit::textChanged, this,
            &NationalSquadSelectionPage::rebuildLists);
    connect(addButton, &QPushButton::clicked, this,
            [this] { moveSelected(m_availableList, true); });
    connect(removeButton, &QPushButton::clicked, this,
            [this] { moveSelected(m_squadList, false); });
    connect(m_availableList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { moveSelected(m_availableList, true); });
    connect(m_squadList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { moveSelected(m_squadList, false); });
    connect(m_saveButton, &QPushButton::clicked, this, &NationalSquadSelectionPage::save);
}

void NationalSquadSelectionPage::refresh()
{
    // Keep unsaved in-progress edits across dataChanged refreshes; otherwise
    // mirror the saved squad.
    if (!m_dirty)
        m_selection = nationalSquadUids(m_context);
    rebuildLists();
}

void NationalSquadSelectionPage::rebuildLists()
{
    const QString name = m_context.nationalTeamName();
    const QString code = m_context.nationalTeamCode();
    const int ageLimit = m_context.nationalTeamAgeLimit();

    if (name.isEmpty() || code.isEmpty() || ageLimit <= 0) {
        m_hint->setText(
            tr("⚠️ Bitte konfiguriere zuerst dein Nationalteam vollständig unter "
               "Einstellungen → Verein (Name, Länder-Code, Altersgrenze)."));
        m_availableList->clear();
        m_squadList->clear();
        m_saveButton->setEnabled(false);
        return;
    }
    m_saveButton->setEnabled(true);
    m_hint->setText(tr("Pool: Spieler mit Nationalität '%1'%2. Doppelklick oder Buttons zum "
                       "Verschieben; Speichern übernimmt die Auswahl in die Datenbank.")
                        .arg(code, ageLimit < 99 ? tr(" bis Alter %1").arg(ageLimit)
                                                 : QString()));

    const QString query = m_searchEdit->text().trimmed();

    std::vector<const Player *> available, squad;
    for (const Player &player : m_context.store().players()) {
        if (m_selection.contains(player.uid)) {
            // Squad members stay visible even if they aged out or the
            // nationality data changed — otherwise they could not be removed.
            squad.push_back(&player);
            continue;
        }
        const bool eligible = player.nationality == code || player.secondNationality == code;
        if (!eligible)
            continue;
        if (ageLimit < 99 && (player.age <= 0 || player.age > ageLimit))
            continue;
        if (!query.isEmpty() && !player.name.contains(query, Qt::CaseInsensitive))
            continue;
        available.push_back(&player);
    }
    const auto byName = [](const Player *a, const Player *b) {
        return getLastName(a->name).localeAwareCompare(getLastName(b->name)) < 0;
    };
    std::sort(available.begin(), available.end(), byName);
    std::sort(squad.begin(), squad.end(), byName);

    m_availableList->clear();
    for (const Player *player : available)
        playerItem(*player, m_availableList);
    m_squadList->clear();
    for (const Player *player : squad)
        playerItem(*player, m_squadList);

    m_squadTitle->setText(tr("Aktueller Kader (%1 Spieler)%2")
                              .arg(m_selection.size())
                              .arg(m_dirty ? tr(" — ungespeichert") : QString()));
}

void NationalSquadSelectionPage::moveSelected(QListWidget *from, bool adding)
{
    const auto items = from->selectedItems();
    if (items.isEmpty())
        return;
    for (const QListWidgetItem *item : items) {
        const QString uid = item->data(Qt::UserRole).toString();
        if (adding)
            m_selection.insert(uid);
        else
            m_selection.remove(uid);
    }
    m_dirty = true;
    rebuildLists();
}

void NationalSquadSelectionPage::save()
{
    QList<int> ids;
    for (const QString &uid : std::as_const(m_selection)) {
        if (const Player *player = m_context.store().findByUid(uid))
            ids << player->id;
    }
    if (!m_context.database().setNationalSquadIds(ids)) {
        QMessageBox::critical(this, tr("Nationalkader"), m_context.database().errorString());
        return;
    }
    const int count = static_cast<int>(ids.size());
    m_dirty = false;
    m_context.reloadFromDatabase();
    QMessageBox::information(this, tr("Nationalkader"),
                             tr("%1 Spieler im Nationalkader gespeichert.").arg(count));
}

} // namespace fm
