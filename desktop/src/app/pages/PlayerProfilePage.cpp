#include "PlayerProfilePage.h"

#include "../AppContext.h"
#include "../RecalcHelper.h"
#include "../widgets/Charts.h"
#include "../widgets/PlayerSearchModel.h"
#include "core/Constants.h"
#include "core/HtmlImporter.h"
#include "core/RoleAnalysis.h"
#include "core/TalentEngine.h"
#include "core/Utils.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCompleter>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

enum Scope { MyClub = 0, SecondTeam = 1, Scouted = 2, All = 3, NationalSquad = 4 };

QString pill(const QString &label, const QString &value, const QString &color,
             const QString &textColor = QStringLiteral("white"))
{
    return QStringLiteral(
               "<span style='background-color:%1; color:%2; padding:3px 10px; "
               "border-radius:12px; font-size:9pt; white-space:nowrap;'>%3: %4</span>&nbsp;")
        .arg(color, textColor, label, value.toHtmlEscaped());
}

QString attrChip(const QString &name, int value)
{
    const CellStyle style = attributeCellStyle(value);
    return QStringLiteral(
               "<span style='background-color:%1; color:%2; padding:2px 8px; "
               "border-radius:6px; font-size:9pt; white-space:nowrap;'>%3: %4</span>&nbsp; ")
        .arg(style.background.name(), style.text.name(), name.toHtmlEscaped())
        .arg(value);
}

} // namespace

PlayerProfilePage::PlayerProfilePage(AppContext &context, ThemeManager &theme, QWidget *parent)
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

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Spieler-Profil")), content);
    layout->addWidget(heading);

    // --- Selection row ---
    auto *selectRow = new QHBoxLayout;
    m_scopeGroup = new QButtonGroup(this);
    const auto addScope = [&](const QString &label, int id, bool checked = false) {
        auto *radio = new QRadioButton(label, content);
        radio->setChecked(checked);
        m_scopeGroup->addButton(radio, id);
        selectRow->addWidget(radio);
    };
    addScope(tr("🏠 Mein Verein"), MyClub);
    addScope(tr("🔄 Zweitteam"), SecondTeam);
    addScope(tr("🔍 Gescoutete"), Scouted);
    addScope(tr("🌟 Nationalkader"), NationalSquad);
    addScope(tr("🌍 Alle Spieler"), All, true);
    selectRow->addSpacing(12);

    // Type-to-search instead of a giant dropdown: fast even with 80k players
    // (the completer model is built lazily on the first keystroke) and matches
    // accent-/umlaut-insensitively.
    m_searchEdit = new QLineEdit(content);
    m_searchEdit->setMinimumWidth(340);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("🔎 Spieler suchen (Name eintippen)…"));
    m_searchModel = new PlayerSearchModel(this);
    m_searchCompleter = new FoldingCompleter(m_searchModel, this);
    m_searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_searchCompleter->setFilterMode(Qt::MatchContains);
    m_searchCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_searchCompleter->setCompletionRole(PlayerSearchKeyRole);
    m_searchCompleter->setMaxVisibleItems(12);
    m_searchEdit->setCompleter(m_searchCompleter);
    selectRow->addWidget(m_searchEdit, 1);
    layout->addLayout(selectRow);

    auto *divider = new QFrame(content);
    divider->setFrameShape(QFrame::HLine);
    layout->addWidget(divider);

    // --- Header block ---
    m_nameLabel = new QLabel(content);
    m_vitalsLabel = new QLabel(content);
    m_tagsLabel = new QLabel(content);
    m_tagsLabel->setTextFormat(Qt::RichText);
    m_tagsLabel->setWordWrap(true);
    layout->addWidget(m_nameLabel);
    layout->addWidget(m_vitalsLabel);
    layout->addWidget(m_tagsLabel);

    // --- Manual single-player update ---
    m_updateBox = new QGroupBox(tr("🔁 Diesen Spieler aus einer HTML-Datei aktualisieren"),
                                content);
    m_updateBox->setCheckable(true);
    m_updateBox->setChecked(false);
    auto *updateInner = new QWidget(m_updateBox);
    auto *updateLayout = new QVBoxLayout(updateInner);
    auto *updateBoxLayout = new QVBoxLayout(m_updateBox);
    updateBoxLayout->addWidget(updateInner);
    connect(m_updateBox, &QGroupBox::toggled, updateInner, &QWidget::setVisible);
    updateInner->setVisible(false);
    auto *updateHint = new QLabel(
        tr("Lade einen FM-Export hoch, der NUR diesen Spieler enthält. Die Daten werden "
           "direkt auf dieses Profil geschrieben — auch wenn die UID in der Datei abweicht "
           "(z. B. fehlendes 'r-'-Präfix). Zugewiesene Rollen, Primärrolle und "
           "Transfer-/Leih-Einstellungen bleiben erhalten."),
        updateInner);
    updateHint->setWordWrap(true);
    updateHint->setObjectName(QStringLiteral("kpiCaption"));
    updateLayout->addWidget(updateHint);
    auto *fileRow = new QHBoxLayout;
    m_updateFileEdit = new QLineEdit(updateInner);
    m_updateFileEdit->setReadOnly(true);
    m_updateFileEdit->setPlaceholderText(tr("Keine Datei ausgewählt…"));
    auto *browse = new QPushButton(tr("Durchsuchen…"), updateInner);
    fileRow->addWidget(m_updateFileEdit, 1);
    fileRow->addWidget(browse);
    updateLayout->addLayout(fileRow);
    auto *confirmRow = new QHBoxLayout;
    m_updateConfirm = new QCheckBox(updateInner);
    m_updateButton = new QPushButton(tr("Spieler aktualisieren"), updateInner);
    m_updateButton->setEnabled(false);
    confirmRow->addWidget(m_updateConfirm, 1);
    confirmRow->addWidget(m_updateButton);
    updateLayout->addLayout(confirmRow);
    layout->addWidget(m_updateBox);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("FM-HTML-Export auswählen"), QString(),
            tr("HTML-Dateien (*.html *.htm);;Alle Dateien (*)"));
        if (!path.isEmpty())
            m_updateFileEdit->setText(path);
        m_updateButton->setEnabled(!m_updateFileEdit->text().isEmpty()
                                   && m_updateConfirm->isChecked());
    });
    connect(m_updateConfirm, &QCheckBox::toggled, this, [this](bool checked) {
        m_updateButton->setEnabled(checked && !m_updateFileEdit->text().isEmpty());
    });
    connect(m_updateButton, &QPushButton::clicked, this, &PlayerProfilePage::runManualUpdate);

    // --- Detail block (top roles, talent, analysis, chart) ---
    m_detailWidget = new QWidget(content);
    auto *detailLayout = new QVBoxLayout(m_detailWidget);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(10);

    auto *topRolesTitle = new QLabel(tr("Top-Rollen"), m_detailWidget);
    topRolesTitle->setObjectName(QStringLiteral("sectionTitle"));
    detailLayout->addWidget(topRolesTitle);
    m_topRolesLayout = new QHBoxLayout;
    m_topRolesLayout->setSpacing(12);
    detailLayout->addLayout(m_topRolesLayout);

    m_talentBox = new QGroupBox(tr("🌱 Talent-Projektion"), m_detailWidget);
    auto *talentLayout = new QVBoxLayout(m_talentBox);
    m_talentLabel = new QLabel(m_talentBox);
    m_talentLabel->setWordWrap(true);
    talentLayout->addWidget(m_talentLabel);
    detailLayout->addWidget(m_talentBox);

    m_analysisTitle = new QLabel(m_detailWidget);
    m_analysisTitle->setObjectName(QStringLiteral("sectionTitle"));
    detailLayout->addWidget(m_analysisTitle);
    auto *analysisRow = new QHBoxLayout;
    auto *prosConsColumn = new QVBoxLayout;
    m_prosLabel = new QLabel(m_detailWidget);
    m_prosLabel->setWordWrap(true);
    m_consLabel = new QLabel(m_detailWidget);
    m_consLabel->setWordWrap(true);
    prosConsColumn->addWidget(m_prosLabel);
    prosConsColumn->addWidget(m_consLabel);
    prosConsColumn->addStretch(1);
    auto *keyColumn = new QVBoxLayout;
    auto *keyTitle = new QLabel(tr("Attribut-Übersicht"), m_detailWidget);
    keyTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_keyAttrsLabel = new QLabel(m_detailWidget);
    m_keyAttrsLabel->setTextFormat(Qt::RichText);
    m_keyAttrsLabel->setWordWrap(true);
    m_keyAttrsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    keyColumn->addWidget(keyTitle);
    keyColumn->addWidget(m_keyAttrsLabel);
    keyColumn->addStretch(1);
    analysisRow->addLayout(prosConsColumn, 1);
    analysisRow->addLayout(keyColumn, 1);
    detailLayout->addLayout(analysisRow);

    auto *chartTitle = new QLabel(tr("DWRS-Entwicklung"), m_detailWidget);
    chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    detailLayout->addWidget(chartTitle);
    m_chart = new LineChartWidget(m_theme, m_detailWidget);
    detailLayout->addWidget(m_chart);

    layout->addWidget(m_detailWidget);
    m_noRolesLabel = new QLabel(
        tr("Dieser Spieler hat noch keine bewerteten Rollen. Weise ihm auf der Seite "
           "'Rollen zuweisen' Rollen zu."),
        content);
    m_noRolesLabel->hide();
    layout->addWidget(m_noRolesLabel);
    layout->addStretch(1);

    connect(m_scopeGroup, &QButtonGroup::idClicked, this, [this] {
        if (!m_updating)
            onScopeChanged();
    });
    // Build the (potentially huge) completer model only once the user starts
    // typing, then reuse it until the scope or the store changes.
    connect(m_searchEdit, &QLineEdit::textEdited, this, [this] {
        if (m_searchModelDirty)
            rebuildSearchModel();
    });
    connect(m_searchCompleter, QOverload<const QModelIndex &>::of(&QCompleter::activated), this,
            [this](const QModelIndex &index) {
                selectPlayer(index.data(Qt::UserRole).toString());
            });
}

void PlayerProfilePage::refresh()
{
    // The store may have changed underneath us; the completer model is rebuilt
    // lazily on the next keystroke.
    m_searchModelDirty = true;

    // One-shot navigation from the global player search.
    const QString target = m_context.pendingProfileUid();
    if (!target.isEmpty()) {
        m_context.setPendingProfileUid(QString());
        m_updating = true;
        m_scopeGroup->button(All)->setChecked(true);
        m_updating = false;
        updateScopeAvailability();
        if (const Player *player = m_context.store().findByUid(target))
            m_searchEdit->setText(QStringLiteral("%1 (%2)").arg(player->name, player->club));
        selectPlayer(target);
        return;
    }

    updateScopeAvailability();
    // Keep showing the current player across data refreshes if it still exists.
    if (m_currentUid.isEmpty() || !m_context.store().findByUid(m_currentUid))
        m_currentUid.clear();
    showPlayer();
}

void PlayerProfilePage::updateScopeAvailability()
{
    const QString secondClub = m_context.secondTeamClub();
    m_scopeGroup->button(SecondTeam)->setVisible(!secondClub.isEmpty());

    bool hasNationalSquad = false;
    for (const Player &player : m_context.store().players()) {
        if (player.inNationalSquad) {
            hasNationalSquad = true;
            break;
        }
    }
    m_scopeGroup->button(NationalSquad)->setVisible(hasNationalSquad);
    if (!hasNationalSquad && m_scopeGroup->checkedId() == NationalSquad) {
        m_updating = true;
        m_scopeGroup->button(All)->setChecked(true);
        m_updating = false;
        m_searchModelDirty = true;
    }
}

void PlayerProfilePage::onScopeChanged()
{
    updateScopeAvailability();
    m_searchModelDirty = true;
    m_searchEdit->clear();
    m_currentUid.clear();
    showPlayer();
}

void PlayerProfilePage::rebuildSearchModel()
{
    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();
    const int scope = m_scopeGroup->checkedId();

    std::vector<const Player *> pool;
    for (const Player &player : m_context.store().players()) {
        const bool isMine = !userClub.isEmpty() && player.club == userClub;
        const bool isSecond = !secondClub.isEmpty() && player.club == secondClub;
        if ((scope == MyClub && !isMine) || (scope == SecondTeam && !isSecond)
            || (scope == Scouted && (isMine || isSecond))
            || (scope == NationalSquad && !player.inNationalSquad))
            continue;
        pool.push_back(&player);
    }
    std::sort(pool.begin(), pool.end(), [](const Player *a, const Player *b) {
        return getLastName(a->name).localeAwareCompare(getLastName(b->name)) < 0;
    });

    // One model reset instead of one QStandardItem + dataChanged per player:
    // the completer re-filters on every source-model change, so item-wise
    // population froze the UI for tens of seconds on large saves.
    std::vector<PlayerSearchModel::Entry> entries;
    entries.reserve(pool.size());
    for (const Player *player : pool) {
        entries.push_back({QStringLiteral("%1 (%2)").arg(player->name, player->club),
                           player->uid,
                           foldForSearch(player->name + QLatin1Char(' ') + player->club)});
    }
    m_searchModel->setEntries(std::move(entries));
    m_searchModelDirty = false;
}

void PlayerProfilePage::selectPlayer(const QString &uid)
{
    m_currentUid = uid;
    showPlayer();
}

void PlayerProfilePage::showPlayer()
{
    const Player *player = m_context.store().findByUid(m_currentUid);

    const bool hasPlayer = player != nullptr;
    m_updateBox->setVisible(hasPlayer);
    m_detailWidget->setVisible(hasPlayer);
    m_noRolesLabel->setVisible(false);
    if (!hasPlayer) {
        m_nameLabel->setText(tr("<h3>Kein Spieler ausgewählt.</h3>"));
        m_vitalsLabel->clear();
        m_tagsLabel->clear();
        return;
    }

    // --- Header ---
    m_nameLabel->setText(QStringLiteral("<h3>%1</h3>").arg(player->name.toHtmlEscaped()));
    QStringList vitals;
    if (player->age > 0)
        vitals << tr("<b>Alter:</b> %1").arg(player->age);
    if (!player->club.isEmpty())
        vitals << tr("<b>Verein:</b> %1").arg(player->club.toHtmlEscaped());
    if (!player->positionRaw.isEmpty())
        vitals << tr("<b>Position:</b> %1").arg(player->positionRaw.toHtmlEscaped());
    m_vitalsLabel->setText(vitals.join(QStringLiteral(" &nbsp;|&nbsp; ")));

    const auto roleNames = m_context.definitions().roleDisplayMap();
    QString tags;
    if (!player->agreedPlayingTime.isEmpty())
        tags += pill(tr("Spielzeit"), player->agreedPlayingTime, QStringLiteral("#0069b3"));
    if (!player->primaryRole.isEmpty())
        tags += pill(tr("Primärrolle"),
                     roleNames.value(player->primaryRole, player->primaryRole),
                     QStringLiteral("#0da025"));
    if (!player->preferredFoot.isEmpty())
        tags += pill(tr("Fuß"), player->preferredFoot, QStringLiteral("#6c5ce7"));
    if (!player->personality.isEmpty()) {
        const CellStyle style = personalityCellStyle(
            m_context.definitions().personalityCategory(player->personality));
        tags += style.isValid()
                    ? pill(tr("Persönlichkeit"), player->personality,
                           style.background.name(), style.text.name())
                    : pill(tr("Persönlichkeit"), player->personality, QStringLiteral("#555"));
    }
    m_tagsLabel->setText(tags);
    m_updateConfirm->setText(
        tr("Ja, diese Datei ist sicher ein Update für %1.").arg(player->name));

    // --- Top roles ---
    while (QLayoutItem *item = m_topRolesLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    // role -> uid -> (absolute, normalized), the shape topRolesForPlayer expects.
    QHash<QString, QHash<QString, QPair<double, double>>> ratingsByRole;
    const LatestRatings &latest = m_context.latestRatings();
    for (const QString &role : player->assignedRoles) {
        const auto it = latest.constFind({player->id, role});
        if (it != latest.constEnd())
            ratingsByRole[role].insert(player->uid, it.value());
    }
    const auto topRoles =
        RoleAnalysis::topRolesForPlayer(m_context.definitions(), *player, ratingsByRole, 5);

    if (topRoles.empty()) {
        m_detailWidget->setVisible(false);
        m_noRolesLabel->setVisible(true);
        return;
    }

    int index = 0;
    for (const auto &topRole : topRoles) {
        auto *tile = new QFrame(m_detailWidget);
        tile->setObjectName(QStringLiteral("kpiTile"));
        auto *tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(14, 10, 14, 10);
        auto *value = new QLabel(QStringLiteral("%1%").arg(topRole.normalized), tile);
        value->setObjectName(QStringLiteral("kpiValue"));
        auto *caption = new QLabel(
            (index == 0 ? QStringLiteral("🥇 ") : QString()) + topRole.roleName, tile);
        caption->setObjectName(QStringLiteral("kpiCaption"));
        caption->setWordWrap(true);
        tileLayout->addWidget(value);
        tileLayout->addWidget(caption);
        m_topRolesLayout->addWidget(tile, 1);
        ++index;
    }

    // --- Talent projection (prospects only) ---
    const int outfielderCap = m_context.config().ageThreshold(QStringLiteral("outfielder"));
    const int goalkeeperCap = m_context.config().ageThreshold(QStringLiteral("goalkeeper"));
    const double ageCap = TalentEngine::ageCapForPlayer(*player, outfielderCap, goalkeeperCap);
    if (player->age > 0 && player->age <= ageCap) {
        const double score = TalentEngine::talentForPlayer(
            m_context.definitions(), *player, topRoles.front().normalized, ageCap);
        m_talentBox->setVisible(true);
        m_talentLabel->setText(
            tr("<b style='font-size:14pt;'>Talent-Score (U%1): %2</b><br/>"
               "Bester Rollen-DWRS <b>%3%</b> + Entwicklungs-Spielraum (%1−%4 Jahre) "
               "+ Mentalität (Ent %5 / Arb %6) + Persönlichkeit (%7).")
                .arg(static_cast<int>(ageCap))
                .arg(qRound(score))
                .arg(topRoles.front().normalized)
                .arg(player->age)
                .arg(static_cast<int>(player->attrMean(idx(Attr::Determination))))
                .arg(static_cast<int>(player->attrMean(idx(Attr::WorkRate))))
                .arg(player->personality.isEmpty() ? QStringLiteral("—")
                                                   : player->personality.toHtmlEscaped()));
    } else {
        m_talentBox->setVisible(false);
    }

    // --- Pros & cons + key attributes for the best role ---
    const QString bestRole = topRoles.front().role;
    m_analysisTitle->setText(tr("Analyse als %1").arg(topRoles.front().roleName));
    const RoleAnalysis::RoleReport report = RoleAnalysis::analyzePlayerForRole(
        m_context.definitions(), *player, bestRole, true, true);

    QString prosHtml = QStringLiteral("<b>✅ %1</b><ul>").arg(tr("Stärken"));
    for (const auto &pro : report.pros)
        prosHtml += QStringLiteral("<li>%1</li>")
                        .arg(RoleAnalysis::formatProLine(pro, report.roleName).toHtmlEscaped());
    prosHtml += QStringLiteral("</ul>");
    m_prosLabel->setText(report.pros.empty()
                             ? QStringLiteral("<b>✅ %1</b><p>%2</p>")
                                   .arg(tr("Stärken"), tr("Keine herausragenden Stärken."))
                             : prosHtml);
    QString consHtml = QStringLiteral("<b>⚠️ %1</b><ul>").arg(tr("Schwächen"));
    for (const auto &con : report.cons)
        consHtml += QStringLiteral("<li>%1</li>")
                        .arg(RoleAnalysis::formatConLine(con, report.roleName).toHtmlEscaped());
    consHtml += QStringLiteral("</ul>");
    m_consLabel->setText(report.cons.empty()
                             ? QStringLiteral("<b>⚠️ %1</b><p>%2</p>")
                                   .arg(tr("Schwächen"), tr("Keine nennenswerten Schwächen."))
                             : consHtml);

    const RoleWeights weights = m_context.definitions().roleWeights(bestRole);
    QString chips;
    const auto renderChips = [&](const QStringList &attrs, const QString &title) {
        if (attrs.isEmpty())
            return;
        chips += QStringLiteral("<p><b>%1</b><br/>").arg(title);
        for (const QString &attr : attrs) {
            const int attrIndex = attrIndexByName(attr);
            if (attrIndex < 0)
                continue;
            const int value = qRound(player->attrMean(attrIndex));
            if (value > 0)
                chips += attrChip(attr, value);
        }
        chips += QStringLiteral("</p>");
    };
    renderChips(weights.key, tr("Schlüssel-Attribute"));
    renderChips(weights.preferable, tr("Bevorzugte Attribute"));
    m_keyAttrsLabel->setText(chips.isEmpty() ? tr("Keine Attribut-Definitionen für diese Rolle.")
                                             : chips);

    // --- Development chart for the top 3 roles ---
    QList<LineChartWidget::Series> chartSeries;
    QList<int> ids{player->id};
    for (int i = 0; i < static_cast<int>(topRoles.size()) && i < 3; ++i) {
        const auto history = m_context.database().dwrsHistory(ids, topRoles[i].role);
        if (history.empty())
            continue;
        LineChartWidget::Series series;
        series.name = topRoles[i].roleName;
        for (const DwrsEntry &entry : history) {
            const QDateTime ts = parseDwrsTimestamp(entry.timestamp);
            if (ts.isValid())
                series.points.append(
                    {static_cast<double>(ts.toMSecsSinceEpoch()), entry.normalized});
        }
        if (!series.points.isEmpty())
            chartSeries.append(series);
    }
    if (chartSeries.isEmpty())
        m_chart->clearChart(tr("Noch keine historischen DWRS-Daten. Importiere über die Zeit "
                               "weitere Snapshots, um die Entwicklung zu sehen."));
    else
        m_chart->setSeries(std::move(chartSeries));
}

void PlayerProfilePage::runManualUpdate()
{
    const QString uid = m_currentUid;
    const Player *player = m_context.store().findByUid(uid);
    const QString filePath = m_updateFileEdit->text();
    if (!player || filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Spieler aktualisieren"), file.errorString());
        return;
    }
    const QString html = QString::fromUtf8(file.readAll());

    QString error;
    const QString fileName = HtmlImporter::forceUpdateSinglePlayer(
        html, m_context.database(), m_context.store().players(), uid, &error,
        m_context.fmVersionId());
    if (!error.isEmpty()) {
        QMessageBox::critical(this, tr("Spieler aktualisieren"), error);
        return;
    }

    const QString playerName = player->name;
    m_updateFileEdit->clear();
    m_updateConfirm->setChecked(false);
    m_context.reloadFromDatabase();

    recalcDwrsFor(m_context, this, {uid}, [this, playerName, fileName](const QString &recalcError) {
        if (!recalcError.isEmpty()) {
            QMessageBox::critical(this, tr("Spieler aktualisieren"), recalcError);
            return;
        }
        QString message = tr("✅ %1 wurde aus der Datei aktualisiert.").arg(playerName);
        if (!fileName.isEmpty() && fileName != playerName)
            message += tr(" (Name in der Datei: '%1'.)").arg(fileName);
        QMessageBox::information(this, tr("Spieler aktualisieren"), message);
    });
}

} // namespace fm
