#include "RoleAnalysisPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "../widgets/PersonalityFilterWidget.h"
#include "../widgets/PlayerTableModel.h"
#include "core/RoleAnalysis.h"
#include "core/Utils.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {
enum Scope { MyClub = 0, SecondTeam = 1, Scouted = 2 };
} // namespace

RoleAnalysisPage::RoleAnalysisPage(AppContext &context, QWidget *parent)
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

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Rollen-Analyse")), content);
    layout->addWidget(heading);

    auto *controlRow = new QHBoxLayout;
    controlRow->addWidget(new QLabel(tr("Rolle:"), content));
    m_roleCombo = new QComboBox(content);
    m_roleCombo->setMinimumWidth(320);
    controlRow->addWidget(m_roleCombo);
    controlRow->addStretch(1);
    layout->addLayout(controlRow);

    m_personalityFilter = new PersonalityFilterWidget(m_context.definitions(), content);
    layout->addWidget(m_personalityFilter);

    m_tabs = new QTabWidget(content);
    m_tabs->addTab(buildGroupTab(&m_myClub), tr("🏠 Mein Verein"));
    m_myClub.tabIndex = 0;
    m_tabs->addTab(buildGroupTab(&m_secondTeam), tr("🔄 Zweitteam"));
    m_secondTeam.tabIndex = 1;
    m_tabs->addTab(buildGroupTab(&m_scouted), tr("🔍 Gescoutete Spieler"));
    m_scouted.tabIndex = 2;
    m_tabs->setMinimumHeight(420);
    layout->addWidget(m_tabs, 1);

    // --- Strengths & weaknesses ---
    auto *divider = new QFrame(content);
    divider->setFrameShape(QFrame::HLine);
    layout->addWidget(divider);

    m_prosConsTitle = new QLabel(content);
    m_prosConsTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(m_prosConsTitle);

    m_scopeRow = new QWidget(content);
    auto *scopeLayout = new QHBoxLayout(m_scopeRow);
    scopeLayout->setContentsMargins(0, 0, 0, 0);
    m_scopeGroup = new QButtonGroup(this);
    const auto addScope = [&](const QString &label, int id, bool checked = false) {
        auto *radio = new QRadioButton(label, m_scopeRow);
        radio->setChecked(checked);
        m_scopeGroup->addButton(radio, id);
        scopeLayout->addWidget(radio);
    };
    addScope(tr("🏠 Mein Verein"), MyClub, true);
    addScope(tr("🔄 Zweitteam"), SecondTeam);
    addScope(tr("🔍 Gescoutete"), Scouted);
    scopeLayout->addSpacing(16);
    m_playerCombo = new QComboBox(m_scopeRow);
    m_playerCombo->setMinimumWidth(320);
    scopeLayout->addWidget(m_playerCombo);
    scopeLayout->addStretch(1);
    layout->addWidget(m_scopeRow);

    auto *prosConsRow = new QHBoxLayout;
    m_prosLabel = new QLabel(content);
    m_prosLabel->setWordWrap(true);
    m_prosLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_consLabel = new QLabel(content);
    m_consLabel->setWordWrap(true);
    m_consLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    prosConsRow->addWidget(m_prosLabel, 1);
    prosConsRow->addWidget(m_consLabel, 1);
    layout->addLayout(prosConsRow);

    connect(m_roleCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (m_updating)
            return;
        rebuildTables();
        rebuildPlayerCombo();
    });
    connect(m_personalityFilter, &PersonalityFilterWidget::filterChanged, this, [this] {
        if (!m_updating)
            rebuildTables();
    });
    connect(m_scopeGroup, &QButtonGroup::idClicked, this, [this] {
        if (!m_updating)
            rebuildPlayerCombo();
    });
    connect(m_playerCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuildProsCons();
    });
}

QWidget *RoleAnalysisPage::buildGroupTab(Group *group)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 6, 4, 4);

    group->model = new PlayerTableModel(this);
    group->proxy = new PlayerFilterProxy(group->model, this);
    group->table = new QTableView(widget);
    group->table->setModel(group->proxy);
    group->table->setSortingEnabled(true);
    group->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    group->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    group->table->setAlternatingRowColors(true);
    group->table->verticalHeader()->setVisible(false);
    layout->addWidget(group->table);
    PlayerActions::attachToView(m_context, group->table, group->proxy);
    return widget;
}

QString RoleAnalysisPage::selectedRole() const
{
    return m_roleCombo->currentData().toString();
}

void RoleAnalysisPage::refresh()
{
    m_updating = true;
    const QString previousRole = selectedRole();
    m_roleCombo->clear();
    const auto roleNames = m_context.definitions().roleDisplayMap();
    for (const QString &role : m_context.definitions().validRoles())
        m_roleCombo->addItem(roleNames.value(role, role), role);
    if (!previousRole.isEmpty()) {
        const int index = m_roleCombo->findData(previousRole);
        if (index >= 0)
            m_roleCombo->setCurrentIndex(index);
    }

    // Personalities present in the data.
    QSet<QString> personalitySet;
    for (const Player &player : m_context.store().players()) {
        if (!player.personality.trimmed().isEmpty())
            personalitySet.insert(player.personality);
    }
    QStringList personalities(personalitySet.cbegin(), personalitySet.cend());
    std::sort(personalities.begin(), personalities.end());
    m_personalityFilter->setAvailablePersonalities(personalities);

    m_tabs->setTabVisible(m_secondTeam.tabIndex, !m_context.secondTeamClub().isEmpty());
    m_updating = false;

    rebuildTables();
    rebuildPlayerCombo();
}

void RoleAnalysisPage::rebuildTables()
{
    const QString role = selectedRole();
    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();
    const auto roleRatings = m_context.ratings().value(role);
    const LatestRatings latest = m_context.latestRatings();

    const auto columns = [&]() -> QList<PlayerColumn> {
        const auto normalizedOf = [roleRatings](const Player &p) {
            return roleRatings.value(p.uid, 0.0);
        };
        const auto absoluteOf = [latest, role](const Player &p) {
            return latest.value({p.id, role}).first;
        };
        auto *definitions = &m_context.definitions();
        return {
            {tr("Name"), [](const Player &p) { return p.name; },
             [](const Player &p) { return getLastName(p.name); }, nullptr, {}},
            {tr("Alter"), [](const Player &p) { return p.age; }, nullptr, nullptr,
             Qt::AlignRight | Qt::AlignVCenter},
            {tr("Position"), [](const Player &p) { return p.positionRaw; }, nullptr, nullptr, {}},
            {tr("Persönlichkeit"), [](const Player &p) { return p.personality; }, nullptr,
             [definitions](const Player &p) {
                 return personalityCellStyle(definitions->personalityCategory(p.personality));
             },
             {}},
            {tr("Linker Fuß"), [](const Player &p) { return p.leftFoot; }, nullptr, nullptr, {}},
            {tr("Rechter Fuß"), [](const Player &p) { return p.rightFoot; }, nullptr, nullptr, {}},
            {tr("Größe"), [](const Player &p) { return p.heightRaw; },
             [](const Player &p) { return p.heightCm; }, nullptr,
             Qt::AlignRight | Qt::AlignVCenter},
            {tr("Verein"), [](const Player &p) { return p.club; }, nullptr, nullptr, {}},
            {tr("Marktwert"), [](const Player &p) { return p.transferValueRaw; },
             [](const Player &p) { return p.transferValue; }, nullptr,
             Qt::AlignRight | Qt::AlignVCenter},
            {tr("DWRS (absolut)"),
             [absoluteOf](const Player &p) {
                 return QString::number(absoluteOf(p), 'f', 2);
             },
             [absoluteOf](const Player &p) { return absoluteOf(p); }, nullptr,
             Qt::AlignRight | Qt::AlignVCenter},
            {tr("DWRS"),
             [normalizedOf](const Player &p) {
                 return QStringLiteral("%1%").arg(qRound(normalizedOf(p)));
             },
             [normalizedOf](const Player &p) { return normalizedOf(p); },
             [normalizedOf](const Player &p) { return dwrsCellStyle(normalizedOf(p)); },
             Qt::AlignRight | Qt::AlignVCenter},
        };
    }();

    std::vector<const Player *> mine, second, scouted;
    for (const Player &player : m_context.store().players()) {
        if (!player.assignedRoles.contains(role) || !roleRatings.contains(player.uid))
            continue;
        if (!m_personalityFilter->allows(player.personality))
            continue;
        if (!userClub.isEmpty() && player.club == userClub)
            mine.push_back(&player);
        else if (!secondClub.isEmpty() && player.club == secondClub)
            second.push_back(&player);
        else
            scouted.push_back(&player);
    }

    const auto apply = [&](Group &group, std::vector<const Player *> rows,
                           const QString &tabText) {
        const int count = static_cast<int>(rows.size());
        group.model->setColumns(columns);
        group.model->setRows(std::move(rows));
        group.table->sortByColumn(10, Qt::DescendingOrder);
        group.table->resizeColumnsToContents();
        m_tabs->setTabText(group.tabIndex, tr("%1 (%2)").arg(tabText).arg(count));
    };
    apply(m_myClub, std::move(mine),
          tr("🏠 %1").arg(userClub.isEmpty() ? tr("Mein Verein") : userClub));
    apply(m_secondTeam, std::move(second),
          tr("🔄 %1").arg(secondClub.isEmpty() ? tr("Zweitteam") : secondClub));
    apply(m_scouted, std::move(scouted), tr("🔍 Gescoutete Spieler"));

    const auto roleNames = m_context.definitions().roleDisplayMap();
    m_prosConsTitle->setText(
        tr("Stärken && Schwächen als %1").arg(roleNames.value(role, role)));
}

void RoleAnalysisPage::rebuildPlayerCombo()
{
    m_updating = true;
    m_playerCombo->clear();

    const QString role = selectedRole();
    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();
    const int scope = m_scopeGroup->checkedId();

    std::vector<const Player *> pool;
    for (const Player &player : m_context.store().players()) {
        if (!player.assignedRoles.contains(role))
            continue;
        const bool isMine = !userClub.isEmpty() && player.club == userClub;
        const bool isSecond = !secondClub.isEmpty() && player.club == secondClub;
        if ((scope == MyClub && !isMine) || (scope == SecondTeam && !isSecond)
            || (scope == Scouted && (isMine || isSecond)))
            continue;
        pool.push_back(&player);
    }
    std::sort(pool.begin(), pool.end(), [](const Player *a, const Player *b) {
        return getLastName(a->name).localeAwareCompare(getLastName(b->name)) < 0;
    });

    for (const Player *player : pool) {
        m_playerCombo->addItem(QStringLiteral("%1 (%2)").arg(player->name, player->club),
                               player->uid);
    }
    m_updating = false;
    rebuildProsCons();
}

void RoleAnalysisPage::rebuildProsCons()
{
    const QString uid = m_playerCombo->currentData().toString();
    const Player *player = m_context.store().findByUid(uid);
    if (!player) {
        m_prosLabel->setText(tr("Keine Spieler mit dieser Rolle im gewählten Pool."));
        m_consLabel->clear();
        return;
    }

    const RoleAnalysis::RoleReport report = RoleAnalysis::analyzePlayerForRole(
        m_context.definitions(), *player, selectedRole());

    QString prosHtml = QStringLiteral("<b>✅ %1</b><ul>").arg(tr("Stärken"));
    if (report.pros.empty()) {
        prosHtml = QStringLiteral("<b>✅ %1</b><p>%2</p>")
                       .arg(tr("Stärken"), tr("Keine herausragenden Stärken für diese Rolle."));
    } else {
        for (const auto &pro : report.pros)
            prosHtml += QStringLiteral("<li>%1</li>")
                            .arg(RoleAnalysis::formatProLine(pro, report.roleName).toHtmlEscaped());
        prosHtml += QStringLiteral("</ul>");
    }
    QString consHtml = QStringLiteral("<b>⚠️ %1</b><ul>").arg(tr("Schwächen"));
    if (report.cons.empty()) {
        consHtml = QStringLiteral("<b>⚠️ %1</b><p>%2</p>")
                       .arg(tr("Schwächen"), tr("Keine nennenswerten Schwächen für diese Rolle."));
    } else {
        for (const auto &con : report.cons)
            consHtml += QStringLiteral("<li>%1</li>")
                            .arg(RoleAnalysis::formatConLine(con, report.roleName).toHtmlEscaped());
        consHtml += QStringLiteral("</ul>");
    }
    m_prosLabel->setText(prosHtml);
    m_consLabel->setText(consHtml);
}

} // namespace fm
