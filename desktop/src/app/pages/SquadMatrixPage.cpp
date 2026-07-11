#include "SquadMatrixPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "../widgets/PersonalityFilterWidget.h"
#include "../widgets/PlayerTableModel.h"
#include "core/TalentEngine.h"
#include "core/Utils.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

enum TalentScope { ScopeDomestic = 0, ScopeForeign = 1, ScopeAny = 2 };

QString millions(double value)
{
    return QStringLiteral("%1 Mio. €").arg(QLocale().toString(value / 1'000'000.0, 'f', 1));
}

} // namespace

SquadMatrixPage::SquadMatrixPage(AppContext &context, QWidget *parent)
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

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Squad Matrix")), content);
    layout->addWidget(heading);

    // --- Options row ---
    auto *optionsRow = new QHBoxLayout;
    optionsRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_tacticCombo = new QComboBox(content);
    m_tacticCombo->setMinimumWidth(240);
    optionsRow->addWidget(m_tacticCombo);
    m_extraDetailsCheck = new QCheckBox(tr("Extra-Details"), content);
    m_secondTeamCheck = new QCheckBox(tr("Zweitteam separat anzeigen"), content);
    m_hideRetiredCheck = new QCheckBox(tr("'Retired' ausblenden"), content);
    m_hideRetiredCheck->setChecked(true);
    optionsRow->addWidget(m_extraDetailsCheck);
    optionsRow->addWidget(m_secondTeamCheck);
    optionsRow->addWidget(m_hideRetiredCheck);
    optionsRow->addStretch(1);
    layout->addLayout(optionsRow);

    m_personalityFilter = new PersonalityFilterWidget(m_context.definitions(), content);
    layout->addWidget(m_personalityFilter);

    // --- Talent filter ---
    m_talentBox = new QGroupBox(tr("🌱 Talent-Filter"), content);
    m_talentBox->setCheckable(true);
    m_talentBox->setChecked(false);
    auto *talentLayout = new QVBoxLayout(m_talentBox);
    auto *scopeRow = new QHBoxLayout;
    scopeRow->addWidget(new QLabel(tr("Nationalität:"), m_talentBox));
    m_talentScope = new QButtonGroup(this);
    const auto addScope = [&](const QString &label, int id, bool checked = false) {
        auto *radio = new QRadioButton(label, m_talentBox);
        radio->setChecked(checked);
        m_talentScope->addButton(radio, id);
        scopeRow->addWidget(radio);
    };
    addScope(tr("Inland"), ScopeDomestic);
    addScope(tr("Ausland"), ScopeForeign);
    addScope(tr("Beliebig"), ScopeAny, true);
    scopeRow->addStretch(1);
    talentLayout->addLayout(scopeRow);

    auto *talentSliders = new QHBoxLayout;
    talentSliders->addWidget(new QLabel(tr("Max. Alter:"), m_talentBox));
    m_talentAgeSlider = new QSlider(Qt::Horizontal, m_talentBox);
    m_talentAgeSlider->setRange(15, 24);
    m_talentAgeSlider->setValue(21);
    m_talentAgeLabel = new QLabel(m_talentBox);
    talentSliders->addWidget(m_talentAgeSlider, 1);
    talentSliders->addWidget(m_talentAgeLabel);
    talentSliders->addSpacing(16);
    talentSliders->addWidget(new QLabel(tr("Min. Ent + Arb:"), m_talentBox));
    m_talentMentalitySlider = new QSlider(Qt::Horizontal, m_talentBox);
    m_talentMentalitySlider->setRange(0, 40);
    m_talentMentalitySlider->setValue(20);
    m_talentMentalityLabel = new QLabel(m_talentBox);
    talentSliders->addWidget(m_talentMentalitySlider, 1);
    talentSliders->addWidget(m_talentMentalityLabel);
    talentSliders->addSpacing(16);
    m_talentGoodOnlyCheck = new QCheckBox(tr("Nur gute Persönlichkeiten"), m_talentBox);
    talentSliders->addWidget(m_talentGoodOnlyCheck);
    talentLayout->addLayout(talentSliders);

    auto *talentHint = new QLabel(
        tr("Talent-Score = bester DWRS der angezeigten Rollen + 2 je Jahr unter der "
           "Altersgrenze + (Ent + Arb − 20) / 4 + 3 (gute) / − 5 (schlechte Persönlichkeit)."),
        m_talentBox);
    talentHint->setObjectName(QStringLiteral("kpiCaption"));
    talentHint->setWordWrap(true);
    talentLayout->addWidget(talentHint);
    layout->addWidget(m_talentBox);

    // --- Tables ---
    buildSection(&m_clubSection, tr("Mein Verein"), content, false);
    layout->addWidget(m_clubSection.box);
    buildSection(&m_secondSection, tr("Zweitteam"), content, false);
    layout->addWidget(m_secondSection.box);
    buildSection(&m_scoutedSection, tr("Gescoutete Spieler"), content, true);
    layout->addWidget(m_scoutedSection.box);
    layout->addStretch(1);

    // --- Signals ---
    const auto fullRebuild = [this] {
        if (!m_updating)
            rebuildAll();
    };
    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, fullRebuild);
    connect(m_extraDetailsCheck, &QCheckBox::toggled, this, fullRebuild);
    connect(m_secondTeamCheck, &QCheckBox::toggled, this, fullRebuild);
    connect(m_hideRetiredCheck, &QCheckBox::toggled, this, fullRebuild);
    connect(m_personalityFilter, &PersonalityFilterWidget::filterChanged, this, fullRebuild);
    connect(m_talentBox, &QGroupBox::toggled, this, fullRebuild);
    connect(m_talentScope, &QButtonGroup::idClicked, this, fullRebuild);
    connect(m_talentGoodOnlyCheck, &QCheckBox::toggled, this, fullRebuild);
    connect(m_talentAgeSlider, &QSlider::valueChanged, this, [this, fullRebuild] {
        m_talentAgeLabel->setText(tr("%1 Jahre").arg(m_talentAgeSlider->value()));
        fullRebuild();
    });
    connect(m_talentMentalitySlider, &QSlider::valueChanged, this, [this, fullRebuild] {
        m_talentMentalityLabel->setText(QString::number(m_talentMentalitySlider->value()));
        fullRebuild();
    });
    m_talentAgeLabel->setText(tr("%1 Jahre").arg(m_talentAgeSlider->value()));
    m_talentMentalityLabel->setText(QString::number(m_talentMentalitySlider->value()));

    const auto scoutedRebuild = [this] {
        if (!m_updating)
            rebuildScoutedOnly();
    };
    connect(m_scoutedFilterCombo, &QComboBox::currentIndexChanged, this, scoutedRebuild);
    connect(m_dwrsMinSpin, &QSpinBox::valueChanged, this, scoutedRebuild);
    connect(m_dwrsMaxSpin, &QSpinBox::valueChanged, this, scoutedRebuild);
    connect(m_maxAgeSlider, &QSlider::valueChanged, this, [this, scoutedRebuild] {
        m_maxAgeLabel->setText(tr("%1 Jahre").arg(m_maxAgeSlider->value()));
        scoutedRebuild();
    });
    connect(m_maxValueSlider, &QSlider::valueChanged, this, [this, scoutedRebuild] {
        m_maxValueLabel->setText(millions(m_maxValueSlider->value() * 500'000.0));
        scoutedRebuild();
    });
    m_maxAgeLabel->setText(tr("%1 Jahre").arg(m_maxAgeSlider->value()));
    m_maxValueLabel->setText(millions(m_maxValueSlider->value() * 500'000.0));
}

void SquadMatrixPage::buildSection(TableSection *section, const QString &title, QWidget *parent,
                                   bool scouted)
{
    section->box = new QGroupBox(title, parent);
    auto *layout = new QVBoxLayout(section->box);

    if (scouted) {
        auto *filterRow = new QHBoxLayout;
        filterRow->addWidget(new QLabel(tr("Filter/Sortierung:"), section->box));
        m_scoutedFilterCombo = new QComboBox(section->box);
        m_scoutedFilterCombo->setMinimumWidth(200);
        filterRow->addWidget(m_scoutedFilterCombo);
        filterRow->addWidget(new QLabel(tr("DWRS:"), section->box));
        m_dwrsMinSpin = new QSpinBox(section->box);
        m_dwrsMinSpin->setRange(0, 100);
        m_dwrsMinSpin->setValue(20);
        m_dwrsMaxSpin = new QSpinBox(section->box);
        m_dwrsMaxSpin->setRange(0, 100);
        m_dwrsMaxSpin->setValue(100);
        filterRow->addWidget(m_dwrsMinSpin);
        filterRow->addWidget(new QLabel(QStringLiteral("–"), section->box));
        filterRow->addWidget(m_dwrsMaxSpin);
        filterRow->addWidget(new QLabel(tr("Max. Alter:"), section->box));
        m_maxAgeSlider = new QSlider(Qt::Horizontal, section->box);
        m_maxAgeSlider->setRange(15, 40);
        m_maxAgeSlider->setValue(30);
        m_maxAgeSlider->setMaximumWidth(140);
        m_maxAgeLabel = new QLabel(section->box);
        filterRow->addWidget(m_maxAgeSlider);
        filterRow->addWidget(m_maxAgeLabel);
        filterRow->addWidget(new QLabel(tr("Max. Wert:"), section->box));
        m_maxValueSlider = new QSlider(Qt::Horizontal, section->box);
        m_maxValueSlider->setRange(0, 400); // 0.5 Mio € steps
        m_maxValueSlider->setValue(400);
        m_maxValueSlider->setMaximumWidth(160);
        m_maxValueLabel = new QLabel(section->box);
        filterRow->addWidget(m_maxValueSlider);
        filterRow->addWidget(m_maxValueLabel);
        filterRow->addStretch(1);
        layout->addLayout(filterRow);
    }

    auto *searchRow = new QHBoxLayout;
    section->search = new QLineEdit(section->box);
    section->search->setPlaceholderText(tr("Nach Name suchen…"));
    section->search->setClearButtonEnabled(true);
    auto *exportButton = new QPushButton(tr("CSV exportieren"), section->box);
    searchRow->addWidget(section->search, 1);
    searchRow->addWidget(exportButton);
    layout->addLayout(searchRow);

    section->model = new PlayerTableModel(this);
    section->proxy = new PlayerFilterProxy(section->model, this);
    section->table = new QTableView(section->box);
    section->table->setModel(section->proxy);
    section->table->setSortingEnabled(true);
    section->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    section->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    section->table->setAlternatingRowColors(true);
    section->table->verticalHeader()->setVisible(false);
    section->table->setMinimumHeight(scouted ? 460 : 340);
    layout->addWidget(section->table);

    connect(section->search, &QLineEdit::textChanged, this,
            [proxy = section->proxy](const QString &text) { proxy->setNameFilter(text); });
    connect(exportButton, &QPushButton::clicked, this,
            [this, section] { exportCsv(*section); });

    // Right-click menu + double-click profile on every table; the scouted
    // star column keeps its single-click shortlist toggle.
    PlayerActions::attachToView(m_context, section->table, section->proxy,
                                scouted ? 0 : -1);
    if (scouted) {
        connect(section->table, &QTableView::clicked, this,
                [this, section](const QModelIndex &index) {
                    if (index.column() != 0)
                        return;
                    if (const Player *player = section->proxy->playerAt(index.row()))
                        toggleShortlist(player);
                });
    }
}

QStringList SquadMatrixPage::selectedRoles() const
{
    const QString tactic = m_tacticCombo->currentData().toString();
    QStringList roles;
    if (tactic.isEmpty()) { // "Alle Rollen"
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

QList<PlayerColumn> SquadMatrixPage::buildColumns(bool clubTable, bool withShortlist) const
{
    const bool talentOn = m_talentBox->isChecked();
    const bool extraDetails = m_extraDetailsCheck->isChecked();
    const QStringList roles = selectedRoles();
    const RoleRatings &ratings = m_context.ratings();
    const Definitions *definitions = &m_context.definitions();
    const QHash<QString, double> talentScores = m_talentScores;

    QList<PlayerColumn> columns;
    if (withShortlist) {
        columns.append({tr("★"),
                        [](const Player &p) {
                            return p.onShortlist ? QStringLiteral("★") : QStringLiteral("☆");
                        },
                        [](const Player &p) { return p.onShortlist ? 1 : 0; }, nullptr,
                        Qt::AlignCenter});
    }
    columns.append({tr("Name"), [](const Player &p) { return p.name; },
                    [](const Player &p) { return getLastName(p.name); }, nullptr, {}});
    columns.append({tr("Alter"), [](const Player &p) { return p.age; }, nullptr, nullptr,
                    Qt::AlignRight | Qt::AlignVCenter});
    if (talentOn) {
        columns.append({tr("Talent"),
                        [talentScores](const Player &p) {
                            return qRound(talentScores.value(p.uid, 0.0));
                        },
                        nullptr,
                        [talentScores](const Player &p) {
                            return dwrsCellStyle(talentScores.value(p.uid, 0.0));
                        },
                        Qt::AlignRight | Qt::AlignVCenter});
    }
    columns.append(
        {tr("Position"), [](const Player &p) { return p.positionRaw; }, nullptr, nullptr, {}});
    columns.append({tr("Persönlichkeit"), [](const Player &p) { return p.personality; },
                    nullptr,
                    [definitions](const Player &p) {
                        return personalityCellStyle(
                            definitions->personalityCategory(p.personality));
                    },
                    {}});
    if (!clubTable) {
        columns.append(
            {tr("Verein"), [](const Player &p) { return p.club; }, nullptr, nullptr, {}});
        columns.append({tr("Marktwert"), [](const Player &p) { return p.transferValueRaw; },
                        [](const Player &p) { return p.transferValue; }, nullptr,
                        Qt::AlignRight | Qt::AlignVCenter});
    }
    columns.append({tr("Gehalt"), [](const Player &p) { return p.wageRaw; }, nullptr, nullptr,
                    Qt::AlignRight | Qt::AlignVCenter});
    if (extraDetails) {
        columns.append({tr("Linker Fuß"), [](const Player &p) { return p.leftFoot; }, nullptr,
                        nullptr, {}});
        columns.append({tr("Rechter Fuß"), [](const Player &p) { return p.rightFoot; }, nullptr,
                        nullptr, {}});
        columns.append({tr("Größe"), [](const Player &p) { return p.heightRaw; },
                        [](const Player &p) { return p.heightCm; }, nullptr,
                        Qt::AlignRight | Qt::AlignVCenter});
        if (clubTable) {
            columns.append({tr("Marktwert"),
                            [](const Player &p) { return p.transferValueRaw; },
                            [](const Player &p) { return p.transferValue; }, nullptr,
                            Qt::AlignRight | Qt::AlignVCenter});
        }
    }
    if (talentOn) {
        const auto attrColumn = [](const QString &header, Attr attr) {
            const int attrIndex = idx(attr);
            return PlayerColumn{header,
                                [attrIndex](const Player &p) {
                                    return static_cast<int>(p.attrMean(attrIndex));
                                },
                                nullptr,
                                [attrIndex](const Player &p) {
                                    return attributeCellStyle(
                                        static_cast<int>(p.attrMean(attrIndex)));
                                },
                                Qt::AlignRight | Qt::AlignVCenter};
        };
        columns.append(attrColumn(tr("Ent"), Attr::Determination));
        columns.append(attrColumn(tr("Arb"), Attr::WorkRate));
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

void SquadMatrixPage::refresh()
{
    m_updating = true;

    // Tactic list: favorites first (legacy order), then the rest.
    const QString previous = m_tacticCombo->currentData().toString();
    const bool hadSelection = m_tacticCombo->count() > 0;
    const QString fav1 = m_context.database().setting(QStringLiteral("favorite_tactic_1"));
    const QString fav2 = m_context.database().setting(QStringLiteral("favorite_tactic_2"));
    QStringList tactics = m_context.definitions().tacticNames();
    std::sort(tactics.begin(), tactics.end());
    QStringList ordered;
    if (!fav1.isEmpty() && tactics.contains(fav1))
        ordered << fav1;
    if (!fav2.isEmpty() && tactics.contains(fav2) && fav2 != fav1)
        ordered << fav2;
    for (const QString &tactic : tactics) {
        if (!ordered.contains(tactic))
            ordered << tactic;
    }
    m_tacticCombo->clear();
    m_tacticCombo->addItem(tr("Alle Rollen"), QString());
    for (const QString &tactic : ordered)
        m_tacticCombo->addItem(tactic, tactic);
    if (hadSelection && !previous.isEmpty()) {
        const int index = m_tacticCombo->findData(previous);
        m_tacticCombo->setCurrentIndex(index >= 0 ? index : 0);
    } else if (!hadSelection && !fav1.isEmpty()) {
        m_tacticCombo->setCurrentIndex(1); // first favorite (legacy default)
    }

    QSet<QString> personalitySet;
    for (const Player &player : m_context.store().players()) {
        if (!player.personality.trimmed().isEmpty())
            personalitySet.insert(player.personality);
    }
    QStringList personalities(personalitySet.cbegin(), personalitySet.cend());
    std::sort(personalities.begin(), personalities.end());
    m_personalityFilter->setAvailablePersonalities(personalities);

    const QString clubCountry = m_context.database().setting(QStringLiteral("club_country_code"));
    if (auto *domestic = m_talentScope->button(ScopeDomestic)) {
        domestic->setText(clubCountry.isEmpty() ? tr("Inland (Land fehlt in Einstellungen)")
                                                : tr("Inland (%1)").arg(clubCountry));
        domestic->setEnabled(!clubCountry.isEmpty());
        if (clubCountry.isEmpty() && domestic->isChecked())
            m_talentScope->button(ScopeAny)->setChecked(true);
    }

    m_updating = false;
    rebuildAll();
}

void SquadMatrixPage::rebuildAll()
{
    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();
    const QStringList roles = selectedRoles();
    const RoleRatings &ratings = m_context.ratings();
    const bool talentOn = m_talentBox->isChecked();
    const bool hideRetired = m_hideRetiredCheck->isChecked();
    const QString clubCountry = m_context.database().setting(QStringLiteral("club_country_code"));

    const int talentAgeCap = m_talentAgeSlider->value();
    const int talentMinMentality = m_talentMentalitySlider->value();
    const bool talentGoodOnly = m_talentGoodOnlyCheck->isChecked();
    const int talentScope = m_talentScope->checkedId();

    m_talentScores.clear();
    const auto passesTalent = [&](const Player &p) -> bool {
        if (!talentOn)
            return true;
        if (p.age <= 0 || p.age > talentAgeCap)
            return false;
        const double mentality =
            p.attrMean(idx(Attr::Determination)) + p.attrMean(idx(Attr::WorkRate));
        if (mentality < talentMinMentality)
            return false;
        if (talentScope == ScopeDomestic) {
            if (p.nationality != clubCountry && p.secondNationality != clubCountry)
                return false;
        } else if (talentScope == ScopeForeign) {
            if (p.nationality.isEmpty())
                return false;
            if (!clubCountry.isEmpty()
                && (p.nationality == clubCountry || p.secondNationality == clubCountry))
                return false;
        }
        if (talentGoodOnly
            && m_context.definitions().personalityCategory(p.personality)
                   != QLatin1String("good")) {
            return false;
        }
        return true;
    };

    std::vector<const Player *> mine, second, scouted;
    for (const Player &player : m_context.store().players()) {
        if (hideRetired
            && player.club.compare(QLatin1String("retired"), Qt::CaseInsensitive) == 0)
            continue;
        if (!m_personalityFilter->allows(player.personality))
            continue;
        if (!passesTalent(player))
            continue;
        if (talentOn) {
            double bestDwrs = 0.0;
            for (const QString &role : roles) {
                const auto roleIt = ratings.constFind(role);
                if (roleIt != ratings.constEnd())
                    bestDwrs = std::max(bestDwrs, roleIt.value().value(player.uid, 0.0));
            }
            m_talentScores.insert(player.uid,
                                  TalentEngine::talentForPlayer(m_context.definitions(), player,
                                                                bestDwrs, talentAgeCap));
        }
        if (!userClub.isEmpty() && player.club == userClub)
            mine.push_back(&player);
        else if (!secondClub.isEmpty() && player.club == secondClub)
            second.push_back(&player);
        else
            scouted.push_back(&player);
    }

    const auto lastNameSort = [](std::vector<const Player *> &rows) {
        std::sort(rows.begin(), rows.end(), [](const Player *a, const Player *b) {
            const int cmp = getLastName(a->name).localeAwareCompare(getLastName(b->name));
            return cmp != 0 ? cmp < 0 : a->name.localeAwareCompare(b->name) < 0;
        });
    };
    lastNameSort(mine);
    lastNameSort(second);

    // --- Club (optionally combined with the second team). ---
    const bool separateSecond = m_secondTeamCheck->isChecked() && !secondClub.isEmpty();
    std::vector<const Player *> clubRows = mine;
    if (!separateSecond && !second.empty()) {
        clubRows.insert(clubRows.end(), second.begin(), second.end());
        lastNameSort(clubRows);
    }
    const QString clubName = userClub.isEmpty() ? tr("Mein Verein") : userClub;
    m_clubSection.box->setTitle(
        (!separateSecond && !second.empty())
            ? tr("Spieler von %1 & Zweitteam (%2)").arg(clubName).arg(clubRows.size())
            : tr("Spieler von %1 (%2)").arg(clubName).arg(clubRows.size()));
    m_clubSection.model->setColumns(buildColumns(true, false));
    m_clubSection.model->setRows(std::move(clubRows));
    m_clubSection.table->resizeColumnsToContents();

    m_secondSection.box->setVisible(separateSecond);
    if (separateSecond) {
        m_secondSection.box->setTitle(
            tr("Spieler von %1 (Zweitteam, %2)").arg(secondClub).arg(second.size()));
        m_secondSection.model->setColumns(buildColumns(true, false));
        m_secondSection.model->setRows(std::move(second));
        m_secondSection.table->resizeColumnsToContents();
    }

    // --- Scouted filter combo (keep selection across rebuilds). ---
    m_updating = true;
    const auto roleNames = m_context.definitions().roleDisplayMap();
    const QString previousFilter = m_scoutedFilterCombo->currentData().toString();
    m_scoutedFilterCombo->clear();
    m_scoutedFilterCombo->addItem(tr("Name"), QStringLiteral("__name"));
    m_scoutedFilterCombo->addItem(tr("Shortlist"), QStringLiteral("__shortlist"));
    if (talentOn)
        m_scoutedFilterCombo->addItem(tr("Talent"), QStringLiteral("__talent"));
    for (const QString &role : roles)
        m_scoutedFilterCombo->addItem(roleNames.value(role, role), role);
    const int previousIndex = m_scoutedFilterCombo->findData(previousFilter);
    if (previousIndex >= 0)
        m_scoutedFilterCombo->setCurrentIndex(previousIndex);
    m_updating = false;

    m_scoutedPool = std::move(scouted);
    rebuildScoutedOnly();
}

void SquadMatrixPage::rebuildScoutedOnly()
{
    const RoleRatings &ratings = m_context.ratings();
    const QString filterKey = m_scoutedFilterCombo->currentData().toString();
    const int minDwrs = m_dwrsMinSpin->value();
    const int maxDwrs = m_dwrsMaxSpin->value();
    const int maxAge = m_maxAgeSlider->value();
    const double maxValue = m_maxValueSlider->value() * 500'000.0;

    std::vector<const Player *> rows;
    if (filterKey == QLatin1String("__shortlist")) {
        for (const Player *player : m_scoutedPool) {
            if (player->onShortlist)
                rows.push_back(player);
        }
    } else {
        const bool roleFilter = !filterKey.startsWith(QLatin1String("__"));
        const QHash<QString, double> roleRatings =
            roleFilter ? ratings.value(filterKey) : QHash<QString, double>();
        for (const Player *player : m_scoutedPool) {
            if (player->age > maxAge || player->transferValue > maxValue)
                continue;
            if (roleFilter) {
                const auto it = roleRatings.constFind(player->uid);
                if (it == roleRatings.constEnd() || it.value() < minDwrs
                    || it.value() > maxDwrs)
                    continue;
            }
            rows.push_back(player);
        }
    }

    m_scoutedSection.box->setTitle(tr("Gescoutete Spieler (%1)").arg(rows.size()));
    m_scoutedSection.model->setColumns(buildColumns(false, true));
    m_scoutedSection.model->setRows(std::move(rows));

    // Sort by the chosen role/talent column, otherwise by name.
    int sortColumn = 1; // Name (column 0 is the shortlist star)
    Qt::SortOrder order = Qt::AscendingOrder;
    const QString wantedHeader = filterKey == QLatin1String("__talent")
                                     ? tr("Talent")
                                     : (!filterKey.startsWith(QLatin1String("__"))
                                            ? filterKey
                                            : QString());
    if (!wantedHeader.isEmpty()) {
        for (int c = 0; c < m_scoutedSection.model->columnCount(); ++c) {
            if (m_scoutedSection.model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString()
                == wantedHeader) {
                sortColumn = c;
                order = Qt::DescendingOrder;
                break;
            }
        }
    }
    m_scoutedSection.table->sortByColumn(sortColumn, order);
    m_scoutedSection.table->resizeColumnsToContents();
}

void SquadMatrixPage::toggleShortlist(const Player *player)
{
    const int row = m_context.store().rowByUid(player->uid);
    if (row < 0)
        return;
    Player &mutablePlayer = m_context.store().at(row);
    mutablePlayer.onShortlist = !mutablePlayer.onShortlist;

    QList<int> ids;
    for (const Player &p : m_context.store().players()) {
        if (p.onShortlist)
            ids << p.id;
    }
    if (!m_context.database().setShortlistIds(ids)) {
        mutablePlayer.onShortlist = !mutablePlayer.onShortlist;
        QMessageBox::critical(this, tr("Shortlist"), m_context.database().errorString());
        return;
    }
    rebuildScoutedOnly();
}

void SquadMatrixPage::exportCsv(const TableSection &section)
{
    const QString path = QFileDialog::getSaveFileName(this, tr("CSV exportieren"),
                                                      QStringLiteral("matrix.csv"),
                                                      tr("CSV-Dateien (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("CSV exportieren"), file.errorString());
        return;
    }
    file.write("\xEF\xBB\xBF", 3); // UTF-8 BOM for Excel
    file.write(section.model->toCsv(section.proxy->orderedPlayers()).toUtf8());
}

} // namespace fm
