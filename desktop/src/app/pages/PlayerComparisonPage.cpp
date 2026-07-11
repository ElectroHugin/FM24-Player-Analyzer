#include "PlayerComparisonPage.h"

#include "../AppContext.h"
#include "../widgets/Charts.h"
#include "core/Constants.h"
#include "core/Utils.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace fm {

namespace {

using CategoryList = QList<QPair<QString, QStringList>>;

// Legacy gameplay-area groupings (player_comparison.py).
CategoryList outfieldGameplayAreas()
{
    return {
        {QStringLiteral("Tempo"),
         {QStringLiteral("Acceleration"), QStringLiteral("Pace")}},
        {QStringLiteral("Abschluss"),
         {QStringLiteral("Finishing"), QStringLiteral("Long Shots")}},
        {QStringLiteral("Passspiel"),
         {QStringLiteral("Passing"), QStringLiteral("Crossing"), QStringLiteral("Vision")}},
        {QStringLiteral("Dribbling"),
         {QStringLiteral("Dribbling"), QStringLiteral("First Touch"), QStringLiteral("Flair")}},
        {QStringLiteral("Verteidigung"),
         {QStringLiteral("Tackling"), QStringLiteral("Marking"), QStringLiteral("Positioning")}},
        {QStringLiteral("Physis"),
         {QStringLiteral("Strength"), QStringLiteral("Stamina"), QStringLiteral("Balance")}},
        {QStringLiteral("Mentalität"),
         {QStringLiteral("Work Rate"), QStringLiteral("Determination"),
          QStringLiteral("Teamwork"), QStringLiteral("Decisions")}},
    };
}

CategoryList gkGameplayAreas()
{
    return {
        {QStringLiteral("Paraden"),
         {QStringLiteral("Reflexes"), QStringLiteral("One vs One"), QStringLiteral("Handling"),
          QStringLiteral("Agility")}},
        {QStringLiteral("Luftkontrolle"),
         {QStringLiteral("Aerial Reach"), QStringLiteral("Command of Area"),
          QStringLiteral("Jumping Reach")}},
        {QStringLiteral("Abschlag/Abwurf"),
         {QStringLiteral("Kicking"), QStringLiteral("Throwing"), QStringLiteral("Passing"),
          QStringLiteral("Vision")}},
        {QStringLiteral("Mitspielen"),
         {QStringLiteral("Rushing Out (Tendency)"), QStringLiteral("Acceleration"),
          QStringLiteral("Pace")}},
        {QStringLiteral("Mentalität"),
         {QStringLiteral("Composure"), QStringLiteral("Concentration"),
          QStringLiteral("Decisions"), QStringLiteral("Anticipation")}},
    };
}

QStringList attrsInCategory(const QHash<QString, QString> &categories, const QString &category)
{
    QStringList attrs;
    for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
        if (it.value() == category)
            attrs << it.key();
    }
    std::sort(attrs.begin(), attrs.end());
    return attrs;
}

} // namespace

PlayerComparisonPage::PlayerComparisonPage(AppContext &context, ThemeManager &theme,
                                           QWidget *parent)
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

    auto *heading =
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Spieler-Vergleich")), content);
    layout->addWidget(heading);

    // --- Filter row ---
    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_tacticCombo = new QComboBox(content);
    m_tacticCombo->setMinimumWidth(200);
    filterRow->addWidget(m_tacticCombo);
    filterRow->addWidget(new QLabel(tr("Rolle:"), content));
    m_roleCombo = new QComboBox(content);
    m_roleCombo->setMinimumWidth(260);
    filterRow->addWidget(m_roleCombo);
    filterRow->addWidget(new QLabel(tr("Pool:"), content));
    m_poolCombo = new QComboBox(content);
    m_poolCombo->addItem(tr("Mein Verein"), QStringLiteral("club"));
    m_poolCombo->addItem(tr("Nationalkader"), QStringLiteral("national"));
    m_poolCombo->addItem(tr("Alle Spieler"), QStringLiteral("all"));
    filterRow->addWidget(m_poolCombo);
    filterRow->addStretch(1);
    layout->addLayout(filterRow);

    m_hint = new QLabel(tr("Bis zu 5 Spieler ankreuzen, um sie zu vergleichen."), content);
    m_hint->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(m_hint);

    m_playerList = new QListWidget(content);
    m_playerList->setMaximumHeight(170);
    layout->addWidget(m_playerList);

    // --- Radar charts ---
    auto *chartsRow = new QHBoxLayout;
    auto *leftColumn = new QVBoxLayout;
    m_gameplayTitle = new QLabel(tr("Spielbereiche"), content);
    m_gameplayTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_gameplayChart = new RadarChartWidget(m_theme, content);
    leftColumn->addWidget(m_gameplayTitle);
    leftColumn->addWidget(m_gameplayChart);
    auto *rightColumn = new QVBoxLayout;
    m_metaTitle = new QLabel(tr("Meta-Attribut-Profil"), content);
    m_metaTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_metaChart = new RadarChartWidget(m_theme, content);
    rightColumn->addWidget(m_metaTitle);
    rightColumn->addWidget(m_metaChart);
    chartsRow->addLayout(leftColumn, 1);
    chartsRow->addLayout(rightColumn, 1);
    layout->addLayout(chartsRow);

    // --- Detailed attribute table ---
    auto *tableTitle = new QLabel(tr("Detaillierter Attribut-Vergleich"), content);
    tableTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(tableTitle);
    m_detailTable = new QTableWidget(content);
    m_detailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_detailTable->setAlternatingRowColors(true);
    m_detailTable->setMinimumHeight(520);
    layout->addWidget(m_detailTable);
    layout->addStretch(1);

    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuildRoleCombo();
    });
    connect(m_roleCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuildPlayerList();
    });
    connect(m_poolCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuildPlayerList();
    });
    connect(m_playerList, &QListWidget::itemChanged, this, [this] {
        if (!m_updating)
            rebuildComparison();
    });
}

void PlayerComparisonPage::refresh()
{
    m_updating = true;
    // In national mode default the pool to the national squad (legacy).
    if (m_context.nationalUiMode()
        && m_poolCombo->currentData().toString() == QLatin1String("club")) {
        m_poolCombo->setCurrentIndex(1);
    }

    // One-shot handoff from the player context menu ("Zum Vergleich"): make
    // sure the player is reachable — pool "Alle Spieler", and a role he has.
    const QStringList pending = m_context.takePendingComparisonUids();
    if (!pending.isEmpty()) {
        m_pendingCheck += pending;
        m_poolCombo->setCurrentIndex(m_poolCombo->findData(QStringLiteral("all")));
        if (const Player *player = m_context.store().findByUid(pending.first())) {
            const QString currentRole = m_roleCombo->currentData().toString();
            if (!player->assignedRoles.contains(currentRole)
                && !player->assignedRoles.isEmpty()) {
                // Switch to "Alle Rollen" so every role is selectable, then
                // pick the player's first assigned role after the combo fill.
                m_tacticCombo->setCurrentIndex(0);
            }
        }
    }
    const QString previous = m_tacticCombo->currentData().toString();
    const bool hadSelection = m_tacticCombo->count() > 0;
    QStringList tactics = m_context.definitions().tacticNames();
    std::sort(tactics.begin(), tactics.end());
    m_tacticCombo->clear();
    m_tacticCombo->addItem(tr("Alle Rollen"), QString());
    for (const QString &tactic : tactics)
        m_tacticCombo->addItem(tactic, tactic);
    if (hadSelection && !previous.isEmpty()) {
        const int index = m_tacticCombo->findData(previous);
        m_tacticCombo->setCurrentIndex(index >= 0 ? index : 0);
    } else if (!hadSelection) {
        const QString fav = m_context.database().setting(QStringLiteral("favorite_tactic_1"));
        const int index = m_tacticCombo->findData(fav);
        if (index >= 0)
            m_tacticCombo->setCurrentIndex(index);
    }
    m_updating = false;
    rebuildRoleCombo();
}

void PlayerComparisonPage::rebuildRoleCombo()
{
    m_updating = true;
    QString previous = m_roleCombo->currentData().toString();
    // Pending handoff: prefer a role the handed-over player actually has.
    if (!m_pendingCheck.isEmpty()) {
        if (const Player *player = m_context.store().findByUid(m_pendingCheck.first())) {
            if (!player->assignedRoles.contains(previous)
                && !player->assignedRoles.isEmpty())
                previous = player->assignedRoles.first();
        }
    }
    const QString tactic = m_tacticCombo->currentData().toString();
    QStringList roles;
    if (tactic.isEmpty()) {
        roles = m_context.definitions().validRoles();
    } else {
        const auto positions = m_context.definitions().tacticRoles().value(tactic);
        QSet<QString> roleSet;
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
            roleSet.insert(it.value());
        roles = QStringList(roleSet.cbegin(), roleSet.cend());
        std::sort(roles.begin(), roles.end());
    }
    const auto roleNames = m_context.definitions().roleDisplayMap();
    m_roleCombo->clear();
    for (const QString &role : roles)
        m_roleCombo->addItem(roleNames.value(role, role), role);
    if (!previous.isEmpty()) {
        const int index = m_roleCombo->findData(previous);
        if (index >= 0)
            m_roleCombo->setCurrentIndex(index);
    }
    m_updating = false;
    rebuildPlayerList();
}

void PlayerComparisonPage::rebuildPlayerList()
{
    m_updating = true;
    const QStringList previouslySelected = selectedUids();
    QSet<QString> checked(previouslySelected.cbegin(), previouslySelected.cend());
    for (const QString &uid : std::as_const(m_pendingCheck))
        checked.insert(uid);
    m_pendingCheck.clear();
    m_playerList->clear();

    const QString role = m_roleCombo->currentData().toString();
    const QString userClub = m_context.userClub();
    const QString poolKey = m_poolCombo->currentData().toString();

    std::vector<const Player *> pool;
    for (const Player &player : m_context.store().players()) {
        if (poolKey == QLatin1String("club")
            && (userClub.isEmpty() || player.club != userClub))
            continue;
        if (poolKey == QLatin1String("national") && !player.inNationalSquad)
            continue;
        if (!player.assignedRoles.contains(role))
            continue;
        pool.push_back(&player);
    }
    std::sort(pool.begin(), pool.end(), [](const Player *a, const Player *b) {
        return getLastName(a->name).localeAwareCompare(getLastName(b->name)) < 0;
    });

    for (const Player *player : pool) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 (%2)").arg(player->name, player->club), m_playerList);
        item->setData(Qt::UserRole, player->uid);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(checked.contains(player->uid) ? Qt::Checked : Qt::Unchecked);
    }
    m_hint->setText(pool.empty()
                        ? tr("Keine Spieler mit dieser Rolle im gewählten Pool gefunden.")
                        : tr("Bis zu 5 Spieler ankreuzen, um sie zu vergleichen (%1 verfügbar).")
                              .arg(pool.size()));
    m_updating = false;
    rebuildComparison();
}

QStringList PlayerComparisonPage::selectedUids() const
{
    QStringList uids;
    for (int i = 0; i < m_playerList->count(); ++i) {
        const QListWidgetItem *item = m_playerList->item(i);
        if (item->checkState() == Qt::Checked)
            uids << item->data(Qt::UserRole).toString();
    }
    return uids;
}

void PlayerComparisonPage::rebuildComparison()
{
    const QString role = m_roleCombo->currentData().toString();
    const QStringList uids = selectedUids();

    std::vector<const Player *> players;
    for (const QString &uid : uids) {
        if (const Player *player = m_context.store().findByUid(uid))
            players.push_back(player);
    }

    const bool isGk = m_context.definitions().gkRoles().contains(role);
    const RoleWeights weights = m_context.definitions().roleWeights(role);

    // Meta categories: global tiers + the role's key/preferable lists.
    CategoryList gameplay = isGk ? gkGameplayAreas() : outfieldGameplayAreas();
    CategoryList meta;
    if (isGk) {
        meta = {{tr("Höchste Prio"),
                 attrsInCategory(gkStatCategories(), QStringLiteral("Top Importance"))},
                {tr("Hohe Prio"),
                 attrsInCategory(gkStatCategories(), QStringLiteral("High Importance"))},
                {tr("Mittlere Prio"),
                 attrsInCategory(gkStatCategories(), QStringLiteral("Medium Importance"))},
                {tr("Schlüssel"), weights.key},
                {tr("Bevorzugt"), weights.preferable}};
        m_metaTitle->setText(tr("Torwart Meta-Attribut-Profil"));
    } else {
        meta = {{tr("Extrem wichtig"),
                 attrsInCategory(globalStatCategories(), QStringLiteral("Extremely Important"))},
                {tr("Wichtig"),
                 attrsInCategory(globalStatCategories(), QStringLiteral("Important"))},
                {tr("Gut"), attrsInCategory(globalStatCategories(), QStringLiteral("Good"))},
                {tr("Schlüssel"), weights.key},
                {tr("Bevorzugt"), weights.preferable}};
        m_metaTitle->setText(tr("Feldspieler Meta-Attribut-Profil"));
    }

    const auto averageFor = [](const Player &player, const QStringList &attrs) {
        if (attrs.isEmpty())
            return 0.0;
        double sum = 0.0;
        for (const QString &attr : attrs) {
            const int attrIndex = attrIndexByName(attr);
            if (attrIndex >= 0)
                sum += player.attrMean(attrIndex);
        }
        return sum / attrs.size();
    };

    const auto buildTraces = [&](const CategoryList &categories) {
        QList<QPair<QString, QList<double>>> traces;
        for (const Player *player : players) {
            QList<double> values;
            for (const auto &[category, attrs] : categories)
                values << averageFor(*player, attrs);
            traces.append({QStringLiteral("%1 (%2)").arg(player->name, player->club), values});
        }
        return traces;
    };
    QStringList gameplayCats, metaCats;
    for (const auto &[category, attrs] : gameplay)
        gameplayCats << category;
    for (const auto &[category, attrs] : meta)
        metaCats << category;
    m_gameplayChart->setData(gameplayCats, buildTraces(gameplay));
    m_metaChart->setData(metaCats, buildTraces(meta));

    // --- Detailed transposed table: rows = fields/attributes, columns = players.
    m_detailTable->clear();
    m_detailTable->setColumnCount(static_cast<int>(players.size()));
    QStringList headers;
    for (const Player *player : players)
        headers << QStringLiteral("%1 (%2)").arg(player->name, player->club);
    m_detailTable->setHorizontalHeaderLabels(headers);

    struct InfoRow {
        QString label;
        std::function<QString(const Player &)> value;
    };
    const auto roleNames = m_context.definitions().roleDisplayMap();
    const QList<InfoRow> infoRows = {
        {tr("Alter"), [](const Player &p) { return QString::number(p.age); }},
        {tr("Verein"), [](const Player &p) { return p.club; }},
        {tr("Position"), [](const Player &p) { return p.positionRaw; }},
        {tr("Persönlichkeit"), [](const Player &p) { return p.personality; }},
        {tr("Größe"), [](const Player &p) { return p.heightRaw; }},
        {tr("Bevorzugter Fuß"), [](const Player &p) { return p.preferredFoot; }},
        {tr("Gehalt"), [](const Player &p) { return p.wageRaw; }},
        {tr("Marktwert"), [](const Player &p) { return p.transferValueRaw; }},
        {tr("Spielzeit"), [](const Player &p) { return p.agreedPlayingTime; }},
        {tr("Zugewiesene Rollen"),
         [roleNames](const Player &p) {
             QStringList names;
             for (const QString &r : p.assignedRoles)
                 names << r;
             return names.join(QStringLiteral(", "));
         }},
    };

    m_detailTable->setRowCount(static_cast<int>(infoRows.size()) + kAttrCount);
    QStringList rowLabels;
    int row = 0;
    const Definitions *definitions = &m_context.definitions();
    for (const InfoRow &info : infoRows) {
        rowLabels << info.label;
        int column = 0;
        for (const Player *player : players) {
            auto *item = new QTableWidgetItem(info.value(*player));
            if (info.label == tr("Persönlichkeit")) {
                const CellStyle style = personalityCellStyle(
                    definitions->personalityCategory(player->personality));
                if (style.isValid()) {
                    item->setBackground(style.background);
                    item->setForeground(style.text);
                }
            }
            m_detailTable->setItem(row, column++, item);
        }
        ++row;
    }
    for (int attrIndex = 0; attrIndex < kAttrCount; ++attrIndex) {
        rowLabels << attrNames()[attrIndex];
        int column = 0;
        for (const Player *player : players) {
            const int value = qRound(player->attrMean(attrIndex));
            auto *item = new QTableWidgetItem(value > 0 ? QString::number(value)
                                                        : QStringLiteral("–"));
            item->setTextAlignment(Qt::AlignCenter);
            if (value > 0) {
                const CellStyle style = attributeCellStyle(value);
                if (style.isValid()) {
                    item->setBackground(style.background);
                    item->setForeground(style.text);
                }
            }
            m_detailTable->setItem(row, column++, item);
        }
        ++row;
    }
    m_detailTable->setVerticalHeaderLabels(rowLabels);
    m_detailTable->resizeColumnsToContents();
}

} // namespace fm
