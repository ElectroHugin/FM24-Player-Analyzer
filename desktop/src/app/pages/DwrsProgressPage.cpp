#include "DwrsProgressPage.h"

#include "../AppContext.h"
#include "../widgets/Charts.h"
#include "core/Utils.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

enum Mode { SquadOverview = 0, PlayerVsPlayer = 1, Individual = 2 };

qint64 toMsecs(const QString &timestamp)
{
    const QDateTime ts =
        QDateTime::fromString(timestamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return ts.isValid() ? ts.toMSecsSinceEpoch() : -1;
}

} // namespace

DwrsProgressPage::DwrsProgressPage(AppContext &context, ThemeManager &theme, QWidget *parent)
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

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("DWRS-Entwicklung")),
                               content);
    layout->addWidget(heading);
    auto *info = new QLabel(
        tr("Analysiere Entwicklungs-Trends: Kader-Durchschnitt je Rolle, Spieler gegen "
           "Spieler in einer Rolle oder ein einzelner Spieler im Detail."),
        content);
    info->setWordWrap(true);
    info->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(info);

    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(tr("Analyse-Modus:"), content));
    m_modeCombo = new QComboBox(content);
    m_modeCombo->addItem(tr("Kader-Überblick (nach Rolle)"));
    m_modeCombo->addItem(tr("Spieler vs. Spieler (in einer Rolle)"));
    m_modeCombo->addItem(tr("Einzelner Spieler (Detailanalyse)"));
    m_modeCombo->setMinimumWidth(320);
    modeRow->addWidget(m_modeCombo);
    modeRow->addStretch(1);
    layout->addLayout(modeRow);

    m_filterStack = new QStackedWidget(content);
    m_filterStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // --- Mode 1: squad overview ---
    {
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Taktik:"), page));
        m_squadTacticCombo = new QComboBox(page);
        m_squadTacticCombo->setMinimumWidth(240);
        row->addWidget(m_squadTacticCombo);
        row->addStretch(1);
        pageLayout->addLayout(row);
        pageLayout->addWidget(new QLabel(tr("Rollen für das Diagramm:"), page));
        m_squadRoleList = new QListWidget(page);
        m_squadRoleList->setMaximumHeight(140);
        pageLayout->addWidget(m_squadRoleList);
        m_filterStack->addWidget(page);
    }

    // --- Mode 2: player vs player ---
    {
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Taktik:"), page));
        m_pvpTacticCombo = new QComboBox(page);
        m_pvpTacticCombo->setMinimumWidth(240);
        row->addWidget(m_pvpTacticCombo);
        row->addWidget(new QLabel(tr("Rolle:"), page));
        m_pvpRoleCombo = new QComboBox(page);
        m_pvpRoleCombo->setMinimumWidth(260);
        row->addWidget(m_pvpRoleCombo);
        row->addStretch(1);
        pageLayout->addLayout(row);
        pageLayout->addWidget(new QLabel(tr("Spieler zum Vergleichen ankreuzen:"), page));
        m_pvpPlayerList = new QListWidget(page);
        m_pvpPlayerList->setMaximumHeight(140);
        pageLayout->addWidget(m_pvpPlayerList);
        m_filterStack->addWidget(page);
    }

    // --- Mode 3: individual ---
    {
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Spieler:"), page));
        m_individualPlayerCombo = new QComboBox(page);
        m_individualPlayerCombo->setMinimumWidth(300);
        row->addWidget(m_individualPlayerCombo);
        row->addStretch(1);
        pageLayout->addLayout(row);
        pageLayout->addWidget(new QLabel(tr("Rollen für das Diagramm:"), page));
        m_individualRoleList = new QListWidget(page);
        m_individualRoleList->setMaximumHeight(140);
        pageLayout->addWidget(m_individualRoleList);
        m_filterStack->addWidget(page);
    }

    layout->addWidget(m_filterStack);

    m_chartTitle = new QLabel(content);
    m_chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(m_chartTitle);
    m_chart = new LineChartWidget(m_theme, content);
    m_chart->setMinimumHeight(420);
    layout->addWidget(m_chart, 1);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_filterStack->setCurrentIndex(index);
        if (!m_updating)
            rebuildChart();
    });
    const auto onFilterChange = [this] {
        if (!m_updating)
            rebuildFilters();
    };
    connect(m_squadTacticCombo, &QComboBox::currentIndexChanged, this, onFilterChange);
    connect(m_pvpTacticCombo, &QComboBox::currentIndexChanged, this, onFilterChange);
    connect(m_pvpRoleCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuildFilters();
    });
    connect(m_individualPlayerCombo, &QComboBox::currentIndexChanged, this, onFilterChange);
    const auto onCheckChange = [this] {
        if (!m_updating)
            rebuildChart();
    };
    connect(m_squadRoleList, &QListWidget::itemChanged, this, onCheckChange);
    connect(m_pvpPlayerList, &QListWidget::itemChanged, this, onCheckChange);
    connect(m_individualRoleList, &QListWidget::itemChanged, this, onCheckChange);
}

std::vector<const Player *> DwrsProgressPage::clubPlayers() const
{
    std::vector<const Player *> players;
    if (m_context.nationalUiMode()) {
        // National mode: the analysis pool is the saved national squad.
        for (const Player &player : m_context.store().players()) {
            if (player.inNationalSquad)
                players.push_back(&player);
        }
        return players;
    }
    const QString userClub = m_context.userClub();
    if (userClub.isEmpty())
        return players;
    for (const Player &player : m_context.store().players()) {
        if (player.club == userClub)
            players.push_back(&player);
    }
    return players;
}

void DwrsProgressPage::fillTacticCombo(QComboBox *combo)
{
    const QString previous = combo->currentData().toString();
    const bool hadSelection = combo->count() > 0;
    QStringList tactics = m_context.definitions().tacticNames();
    std::sort(tactics.begin(), tactics.end());
    combo->blockSignals(true);
    combo->clear();
    combo->addItem(tr("Alle Rollen"), QString());
    for (const QString &tactic : tactics)
        combo->addItem(tactic, tactic);
    if (hadSelection && !previous.isEmpty()) {
        const int index = combo->findData(previous);
        combo->setCurrentIndex(index >= 0 ? index : 0);
    } else if (!hadSelection) {
        const QString fav = m_context.database().setting(QStringLiteral("favorite_tactic_1"));
        const int index = combo->findData(fav);
        if (index >= 0)
            combo->setCurrentIndex(index);
    }
    combo->blockSignals(false);
}

QStringList DwrsProgressPage::rolesForTactic(const QString &tactic) const
{
    if (tactic.isEmpty())
        return m_context.definitions().validRoles();
    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    QSet<QString> roleSet;
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
        roleSet.insert(it.value());
    QStringList roles(roleSet.cbegin(), roleSet.cend());
    std::sort(roles.begin(), roles.end());
    return roles;
}

QStringList DwrsProgressPage::checkedItems(QListWidget *list) const
{
    QStringList values;
    for (int i = 0; i < list->count(); ++i) {
        const QListWidgetItem *item = list->item(i);
        if (item->checkState() == Qt::Checked)
            values << item->data(Qt::UserRole).toString();
    }
    return values;
}

void DwrsProgressPage::refresh()
{
    m_updating = true;
    fillTacticCombo(m_squadTacticCombo);
    fillTacticCombo(m_pvpTacticCombo);
    m_updating = false;
    rebuildFilters();
}

void DwrsProgressPage::rebuildFilters()
{
    m_updating = true;
    const auto roleNames = m_context.definitions().roleDisplayMap();
    const auto players = clubPlayers();

    const auto fillCheckList = [](QListWidget *list, const QList<QPair<QString, QString>> &items,
                                  const QSet<QString> &checkedDefault) {
        QSet<QString> previous;
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->checkState() == Qt::Checked)
                previous.insert(list->item(i)->data(Qt::UserRole).toString());
        }
        const bool usePrevious = list->count() > 0;
        list->clear();
        for (const auto &[key, label] : items) {
            auto *item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, key);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            const bool check = usePrevious ? previous.contains(key)
                                           : checkedDefault.contains(key);
            item->setCheckState(check ? Qt::Checked : Qt::Unchecked);
        }
    };

    // Mode 1: roles of the selected tactic, all checked by default (legacy).
    {
        const QStringList roles = rolesForTactic(m_squadTacticCombo->currentData().toString());
        QList<QPair<QString, QString>> items;
        QSet<QString> defaults;
        for (const QString &role : roles) {
            items.append({role, roleNames.value(role, role)});
            defaults.insert(role);
        }
        // When the tactic changed the previous checks refer to other roles, so
        // reset to "all checked" whenever the role set differs.
        QSet<QString> current;
        for (int i = 0; i < m_squadRoleList->count(); ++i)
            current.insert(m_squadRoleList->item(i)->data(Qt::UserRole).toString());
        const QSet<QString> wanted(roles.cbegin(), roles.cend());
        if (current != wanted)
            m_squadRoleList->clear();
        fillCheckList(m_squadRoleList, items, defaults);
    }

    // Mode 2: role combo + players with that role.
    {
        const QString previousRole = m_pvpRoleCombo->currentData().toString();
        const QStringList roles = rolesForTactic(m_pvpTacticCombo->currentData().toString());
        m_pvpRoleCombo->blockSignals(true);
        m_pvpRoleCombo->clear();
        for (const QString &role : roles)
            m_pvpRoleCombo->addItem(roleNames.value(role, role), role);
        const int index = m_pvpRoleCombo->findData(previousRole);
        if (index >= 0)
            m_pvpRoleCombo->setCurrentIndex(index);
        m_pvpRoleCombo->blockSignals(false);

        const QString role = m_pvpRoleCombo->currentData().toString();
        QList<QPair<QString, QString>> items;
        for (const Player *player : players) {
            if (player->assignedRoles.contains(role))
                items.append({player->uid,
                              QStringLiteral("%1 (%2)").arg(player->name).arg(player->age)});
        }
        fillCheckList(m_pvpPlayerList, items, {});
    }

    // Mode 3: player combo + his assigned roles (first three checked).
    {
        const QString previousPlayer = m_individualPlayerCombo->currentData().toString();
        auto sorted = players;
        std::sort(sorted.begin(), sorted.end(), [](const Player *a, const Player *b) {
            return getLastName(a->name).localeAwareCompare(getLastName(b->name)) < 0;
        });
        m_individualPlayerCombo->blockSignals(true);
        m_individualPlayerCombo->clear();
        for (const Player *player : sorted)
            m_individualPlayerCombo->addItem(player->name, player->uid);
        const int index = m_individualPlayerCombo->findData(previousPlayer);
        if (index >= 0)
            m_individualPlayerCombo->setCurrentIndex(index);
        m_individualPlayerCombo->blockSignals(false);

        const Player *player =
            m_context.store().findByUid(m_individualPlayerCombo->currentData().toString());
        QList<QPair<QString, QString>> items;
        QSet<QString> defaults;
        if (player) {
            QStringList roles = player->assignedRoles;
            std::sort(roles.begin(), roles.end(),
                      [&roleNames](const QString &a, const QString &b) {
                          return roleNames.value(a, a) < roleNames.value(b, b);
                      });
            int count = 0;
            for (const QString &role : roles) {
                items.append({role, roleNames.value(role, role)});
                if (count++ < 3)
                    defaults.insert(role);
            }
        }
        // Reset when the player changed.
        QSet<QString> current;
        for (int i = 0; i < m_individualRoleList->count(); ++i)
            current.insert(m_individualRoleList->item(i)->data(Qt::UserRole).toString());
        QSet<QString> wanted;
        for (const auto &[key, label] : items)
            wanted.insert(key);
        if (current != wanted)
            m_individualRoleList->clear();
        fillCheckList(m_individualRoleList, items, defaults);
    }

    m_updating = false;
    rebuildChart();
}

void DwrsProgressPage::rebuildChart()
{
    const auto players = clubPlayers();
    if (players.empty()) {
        m_chartTitle->setText(QString());
        m_chart->clearChart(
            tr("Keine Spieler für deinen Verein gefunden. Bitte wähle deinen Verein."));
        return;
    }

    const auto roleNames = m_context.definitions().roleDisplayMap();
    const int mode = m_modeCombo->currentIndex();
    QList<LineChartWidget::Series> chartSeries;

    if (mode == SquadOverview) {
        const QStringList roles = checkedItems(m_squadRoleList);
        for (const QString &role : roles) {
            QList<int> ids;
            for (const Player *player : players) {
                if (player->assignedRoles.contains(role))
                    ids << player->id;
            }
            if (ids.isEmpty())
                continue;
            const auto history = m_context.database().dwrsHistory(ids, role);
            if (history.empty())
                continue;
            // Average normalized DWRS per snapshot timestamp.
            QMap<qint64, QPair<double, int>> perSnapshot;
            for (const DwrsEntry &entry : history) {
                const qint64 ms = toMsecs(entry.timestamp);
                if (ms < 0)
                    continue;
                auto &bucket = perSnapshot[ms];
                bucket.first += entry.normalized;
                bucket.second += 1;
            }
            LineChartWidget::Series series;
            series.name = roleNames.value(role, role);
            for (auto it = perSnapshot.constBegin(); it != perSnapshot.constEnd(); ++it)
                series.points.append({static_cast<double>(it.key()),
                                      it.value().first / it.value().second});
            if (!series.points.isEmpty())
                chartSeries.append(series);
        }
        const QString tactic = m_squadTacticCombo->currentData().toString();
        m_chartTitle->setText(
            tr("Durchschnittliche Kader-Entwicklung für Rollen in '%1'")
                .arg(tactic.isEmpty() ? tr("Alle Rollen") : tactic));
    } else if (mode == PlayerVsPlayer) {
        const QString role = m_pvpRoleCombo->currentData().toString();
        const QStringList uids = checkedItems(m_pvpPlayerList);
        for (const QString &uid : uids) {
            const Player *player = m_context.store().findByUid(uid);
            if (!player)
                continue;
            const auto history = m_context.database().dwrsHistory({player->id}, role);
            LineChartWidget::Series series;
            series.name = QStringLiteral("%1 (%2)").arg(player->name).arg(player->age);
            for (const DwrsEntry &entry : history) {
                const qint64 ms = toMsecs(entry.timestamp);
                if (ms >= 0)
                    series.points.append({static_cast<double>(ms), entry.normalized});
            }
            if (!series.points.isEmpty())
                chartSeries.append(series);
        }
        m_chartTitle->setText(
            tr("Entwicklung als %1").arg(roleNames.value(role, role)));
    } else {
        const Player *player =
            m_context.store().findByUid(m_individualPlayerCombo->currentData().toString());
        const QStringList roles = checkedItems(m_individualRoleList);
        if (player) {
            for (const QString &role : roles) {
                const auto history = m_context.database().dwrsHistory({player->id}, role);
                LineChartWidget::Series series;
                series.name = roleNames.value(role, role);
                for (const DwrsEntry &entry : history) {
                    const qint64 ms = toMsecs(entry.timestamp);
                    if (ms >= 0)
                        series.points.append({static_cast<double>(ms), entry.normalized});
                }
                if (!series.points.isEmpty())
                    chartSeries.append(series);
            }
            m_chartTitle->setText(tr("Entwicklung von %1").arg(player->name));
        }
    }

    if (chartSeries.isEmpty())
        m_chart->clearChart(tr("Keine historischen Daten für die aktuelle Auswahl gefunden."));
    else
        m_chart->setSeries(std::move(chartSeries));
}

} // namespace fm
