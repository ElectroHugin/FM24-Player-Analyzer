#include "NationalSquadMatrixPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "../widgets/PersonalityFilterWidget.h"
#include "../widgets/PlayerTableModel.h"
#include "PageHelpers.h"
#include "core/Utils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

NationalSquadMatrixPage::NationalSquadMatrixPage(AppContext &context, QWidget *parent)
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
        QStringLiteral("<h2>%1</h2>").arg(tr("Nationale Squad Matrix")), content);
    layout->addWidget(heading);

    auto *optionsRow = new QHBoxLayout;
    optionsRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_tacticCombo = new QComboBox(content);
    m_tacticCombo->setMinimumWidth(240);
    optionsRow->addWidget(m_tacticCombo);
    m_extraDetailsCheck = new QCheckBox(tr("Extra-Details (Fuß, Größe)"), content);
    m_hideRetiredCheck = new QCheckBox(tr("'Retired' ausblenden"), content);
    m_hideRetiredCheck->setChecked(true);
    optionsRow->addWidget(m_extraDetailsCheck);
    optionsRow->addWidget(m_hideRetiredCheck);
    optionsRow->addStretch(1);
    layout->addLayout(optionsRow);

    m_personalityFilter = new PersonalityFilterWidget(m_context.definitions(), content);
    layout->addWidget(m_personalityFilter);

    buildSection(&m_squadSection, tr("Aktueller Kader"), content);
    layout->addWidget(m_squadSection.box);
    buildSection(&m_poolSection, tr("Berechtigter Pool (nicht nominiert)"), content);
    layout->addWidget(m_poolSection.box);
    layout->addStretch(1);

    const auto rebuildAll = [this] {
        if (!m_updating)
            rebuild();
    };
    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, rebuildAll);
    connect(m_extraDetailsCheck, &QCheckBox::toggled, this, rebuildAll);
    connect(m_hideRetiredCheck, &QCheckBox::toggled, this, rebuildAll);
    connect(m_personalityFilter, &PersonalityFilterWidget::filterChanged, this, rebuildAll);
}

void NationalSquadMatrixPage::buildSection(Section *section, const QString &title,
                                           QWidget *parent)
{
    section->box = new QGroupBox(title, parent);
    auto *layout = new QVBoxLayout(section->box);
    section->search = new QLineEdit(section->box);
    section->search->setPlaceholderText(tr("Nach Name suchen…"));
    section->search->setClearButtonEnabled(true);
    layout->addWidget(section->search);
    section->model = new PlayerTableModel(this);
    section->proxy = new PlayerFilterProxy(section->model, this);
    section->table = new QTableView(section->box);
    section->table->setModel(section->proxy);
    section->table->setSortingEnabled(true);
    section->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    section->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    section->table->setAlternatingRowColors(true);
    section->table->verticalHeader()->setVisible(false);
    section->table->setMinimumHeight(380);
    layout->addWidget(section->table);
    connect(section->search, &QLineEdit::textChanged, this,
            [proxy = section->proxy](const QString &text) { proxy->setNameFilter(text); });
    PlayerActions::attachToView(m_context, section->table, section->proxy);
}

QStringList NationalSquadMatrixPage::selectedRoles() const
{
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
    }
    return m_context.definitions().sortRolesNaturally(roles);
}

QList<PlayerColumn> NationalSquadMatrixPage::buildColumns() const
{
    const bool extraDetails = m_extraDetailsCheck->isChecked();
    const QStringList roles = selectedRoles();
    const RoleRatings &ratings = m_context.ratings();
    const Definitions *definitions = &m_context.definitions();

    QList<PlayerColumn> columns;
    columns.append({tr("Name"), [](const Player &p) { return p.name; },
                    [](const Player &p) { return getLastName(p.name); }, nullptr, {}});
    columns.append({tr("Alter"), [](const Player &p) { return p.age; }, nullptr, nullptr,
                    Qt::AlignRight | Qt::AlignVCenter});
    columns.append(
        {tr("Position"), [](const Player &p) { return p.positionRaw; }, nullptr, nullptr, {}});
    columns.append({tr("Persönlichkeit"), [](const Player &p) { return p.personality; },
                    nullptr,
                    [definitions](const Player &p) {
                        return personalityCellStyle(
                            definitions->personalityCategory(p.personality));
                    },
                    {}});
    columns.append(
        {tr("Verein"), [](const Player &p) { return p.club; }, nullptr, nullptr, {}});
    if (extraDetails) {
        columns.append({tr("Linker Fuß"), [](const Player &p) { return p.leftFoot; }, nullptr,
                        nullptr, {}});
        columns.append({tr("Rechter Fuß"), [](const Player &p) { return p.rightFoot; },
                        nullptr, nullptr, {}});
        columns.append({tr("Größe"), [](const Player &p) { return p.heightRaw; },
                        [](const Player &p) { return p.heightCm; }, nullptr,
                        Qt::AlignRight | Qt::AlignVCenter});
    }
    for (const QString &role : roles) {
        const QHash<QString, double> roleRatings = ratings.value(role);
        columns.append({role,
                        [roleRatings](const Player &p) -> QVariant {
                            const auto it = roleRatings.constFind(p.uid);
                            if (it == roleRatings.constEnd())
                                return QStringLiteral("–");
                            return qRound(it.value());
                        },
                        [roleRatings](const Player &p) {
                            return roleRatings.value(p.uid, -1.0);
                        },
                        [roleRatings](const Player &p) -> CellStyle {
                            const auto it = roleRatings.constFind(p.uid);
                            if (it == roleRatings.constEnd())
                                return {};
                            return dwrsCellStyle(it.value());
                        },
                        Qt::AlignRight | Qt::AlignVCenter});
    }
    return columns;
}

void NationalSquadMatrixPage::refresh()
{
    m_updating = true;
    const QString previous = m_tacticCombo->currentData().toString();
    const bool hadSelection = m_tacticCombo->count() > 0;
    m_tacticCombo->clear();
    m_tacticCombo->addItem(tr("Alle Rollen"), QString());
    for (const QString &tactic : favoritesFirstTactics(m_context, true))
        m_tacticCombo->addItem(tactic, tactic);
    if (hadSelection && !previous.isEmpty()) {
        const int index = m_tacticCombo->findData(previous);
        m_tacticCombo->setCurrentIndex(index >= 0 ? index : 0);
    } else if (!hadSelection
               && !m_context.database()
                       .setting(QStringLiteral("national_fav_tactic_1"))
                       .isEmpty()) {
        m_tacticCombo->setCurrentIndex(1);
    }

    QSet<QString> personalitySet;
    for (const Player &player : m_context.store().players()) {
        if (!player.personality.trimmed().isEmpty())
            personalitySet.insert(player.personality);
    }
    QStringList personalities(personalitySet.cbegin(), personalitySet.cend());
    std::sort(personalities.begin(), personalities.end());
    m_personalityFilter->setAvailablePersonalities(personalities);

    m_updating = false;
    rebuild();
}

void NationalSquadMatrixPage::rebuild()
{
    const QString code = m_context.nationalTeamCode();
    const int ageLimit = m_context.nationalTeamAgeLimit();
    const bool hideRetired = m_hideRetiredCheck->isChecked();

    std::vector<const Player *> squad, pool;
    for (const Player &player : m_context.store().players()) {
        if (hideRetired
            && player.club.compare(QLatin1String("retired"), Qt::CaseInsensitive) == 0)
            continue;
        if (!m_personalityFilter->allows(player.personality))
            continue;
        const bool eligible = !code.isEmpty()
                              && (player.nationality == code
                                  || player.secondNationality == code)
                              && (ageLimit >= 99 || ageLimit <= 0
                                  || (player.age > 0 && player.age <= ageLimit));
        if (player.inNationalSquad)
            squad.push_back(&player);
        else if (eligible)
            pool.push_back(&player);
    }
    const auto byName = [](const Player *a, const Player *b) {
        return getLastName(a->name).localeAwareCompare(getLastName(b->name)) < 0;
    };
    std::sort(squad.begin(), squad.end(), byName);
    std::sort(pool.begin(), pool.end(), byName);

    m_squadSection.box->setTitle(tr("Aktueller Kader (%1)").arg(squad.size()));
    m_squadSection.model->setColumns(buildColumns());
    m_squadSection.model->setRows(std::move(squad));
    m_squadSection.table->resizeColumnsToContents();

    m_poolSection.box->setTitle(
        tr("Berechtigter Pool — nicht nominiert (%1)").arg(pool.size()));
    m_poolSection.model->setColumns(buildColumns());
    m_poolSection.model->setRows(std::move(pool));
    m_poolSection.table->resizeColumnsToContents();
}

} // namespace fm
