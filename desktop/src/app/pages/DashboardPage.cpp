#include "DashboardPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "../theming/ThemeManager.h"
#include "../widgets/StrengthGridWidget.h"
#include "core/Database.h"
#include "core/Utils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
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
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

// Table item that displays text but sorts by a numeric key.
class NumericItem : public QTableWidgetItem
{
public:
    NumericItem(const QString &text, double sortKey)
        : QTableWidgetItem(text)
        , m_key(sortKey)
    {
    }
    bool operator<(const QTableWidgetItem &other) const override
    {
        if (const auto *numeric = dynamic_cast<const NumericItem *>(&other))
            return m_key < numeric->m_key;
        return QTableWidgetItem::operator<(other);
    }

private:
    double m_key;
};

QString formatMillions(double value)
{
    return QStringLiteral("%1 Mio. €")
        .arg(QLocale().toString(value / 1'000'000.0, 'f', 2));
}

// One "player has left" resolution row. destination = new club, or the status
// strings "FrA" (free agent) / "Retired".
struct DepartureChoice {
    QString uid;
    bool left = false;
    QString destination;
};

// The most common outcome for a departed player: released as a free agent.
inline QString freeAgentTag() { return QStringLiteral("FrA"); }
inline QString retiredTag() { return QStringLiteral("Retired"); }

// Modal dialog to resolve players missing from a full squad export
// (legacy "Action Required: Player Departures" form). Every detected player
// defaults to "left → FrA" since that fits the yearly wave of departing youth;
// the user only unchecks the ones who stayed and edits the high-profile moves.
class DepartureDialog : public QDialog
{
public:
    DepartureDialog(const std::vector<const Player *> &missing, QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(tr("Spieler-Abgänge klären"));
        setMinimumWidth(640);

        auto *layout = new QVBoxLayout(this);
        auto *info = new QLabel(
            tr("Diese Spieler stehen in der Datenbank bei deinem Verein, waren aber "
               "nicht in der hochgeladenen Kader-Datei. Standardmäßig gelten alle als "
               "gegangen und werden zu Free Agents (FrA) — hake die aus, die geblieben "
               "sind, und trage bei den anderen bei Bedarf einen neuen Verein oder "
               "'Retired' ein."),
            this);
        info->setWordWrap(true);
        layout->addWidget(info);

        auto *toggleAll = new QCheckBox(tr("Alle als gegangen markieren"), this);
        toggleAll->setChecked(true);
        layout->addWidget(toggleAll);

        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *content = new QWidget;
        auto *grid = new QGridLayout(content);
        grid->setColumnStretch(2, 1);

        auto *hName = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Spieler")), content);
        auto *hGone = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Gegangen")), content);
        auto *hDest = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Neuer Verein / Status")),
                                 content);
        grid->addWidget(hName, 0, 0);
        grid->addWidget(hGone, 0, 1);
        grid->addWidget(hDest, 0, 2);

        int row = 1;
        for (const Player *player : missing) {
            auto *name = new QLabel(QStringLiteral("<b>%1</b> (%2)")
                                        .arg(player->name.toHtmlEscaped(),
                                             player->club.toHtmlEscaped()),
                                    content);
            auto *gone = new QCheckBox(content);
            gone->setChecked(true);
            auto *dest = new QComboBox(content);
            dest->setEditable(true);
            dest->addItem(freeAgentTag());
            dest->addItem(retiredTag());
            dest->setCurrentText(freeAgentTag());
            dest->setToolTip(tr("Neuer Verein eintippen oder Status wählen "
                                "(FrA = Free Agent, Retired = Karriereende)."));
            connect(gone, &QCheckBox::toggled, dest, &QComboBox::setEnabled);

            grid->addWidget(name, row, 0);
            grid->addWidget(gone, row, 1, Qt::AlignHCenter);
            grid->addWidget(dest, row, 2);
            m_rows.push_back({player->uid, gone, dest});
            ++row;
        }
        scroll->setWidget(content);
        layout->addWidget(scroll, 1);

        connect(toggleAll, &QCheckBox::toggled, this, [this](bool checked) {
            for (const Row &r : m_rows)
                r.gone->setChecked(checked);
        });

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                             this);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Abgänge bestätigen"));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    std::vector<DepartureChoice> choices() const
    {
        std::vector<DepartureChoice> result;
        for (const Row &row : m_rows)
            result.push_back({row.uid, row.gone->isChecked(), row.dest->currentText().trimmed()});
        return result;
    }

private:
    struct Row {
        QString uid;
        QCheckBox *gone;
        QComboBox *dest;
    };
    std::vector<Row> m_rows;
};

} // namespace

DashboardPage::DashboardPage(AppContext &context, ThemeManager &theme, QWidget *parent)
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
    layout->setSpacing(12);

    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Dashboard")), content);
    layout->addWidget(heading);

    layout->addWidget(buildImportSection(content));
    layout->addWidget(buildAnalysisHeader(content));

    m_kpiRow = buildKpiRow(content);
    layout->addWidget(m_kpiRow);

    m_squadSection = buildSquadSection(content);
    layout->addWidget(m_squadSection);

    m_suggestionBox = qobject_cast<QGroupBox *>(buildSuggestionSection(content));
    layout->addWidget(m_suggestionBox);

    layout->addStretch(1);
    scroll->setWidget(content);

    m_suggestionDebounce = new QTimer(this);
    m_suggestionDebounce->setSingleShot(true);
    m_suggestionDebounce->setInterval(180);
    connect(m_suggestionDebounce, &QTimer::timeout, this, &DashboardPage::rebuildSuggestions);
}

QWidget *DashboardPage::buildImportSection(QWidget *parent)
{
    auto *box = new QGroupBox(tr("⬆️ Neue Spielerdaten importieren (FM-HTML-Export)"), parent);
    auto *layout = new QVBoxLayout(box);

    auto *fileRow = new QHBoxLayout;
    m_filePathEdit = new QLineEdit(box);
    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setPlaceholderText(tr("Keine Datei ausgewählt…"));
    auto *browseButton = new QPushButton(tr("Durchsuchen…"), box);
    m_importButton = new QPushButton(tr("Importieren"), box);
    m_importButton->setEnabled(false);
    m_importButton->setDefault(true);
    fileRow->addWidget(m_filePathEdit, 1);
    fileRow->addWidget(browseButton);
    fileRow->addWidget(m_importButton);
    layout->addLayout(fileRow);

    auto *optionsRow = new QHBoxLayout;
    m_squadUpdateCheck = new QCheckBox(
        tr("Datei ist ein kompletter Kader-Export meines Vereins (Abgänge erkennen)"), box);
    m_squadUpdateCheck->setToolTip(
        tr("Aktivieren, wenn die Datei NUR Spieler deines Haupt- und Zweitteams enthält. "
           "Die App erkennt Spieler, die den Verein verlassen haben, und fragt nach."));
    m_autoAssignCheck = new QCheckBox(
        tr("Neuen/unzugeordneten Spielern automatisch Rollen zuweisen"), box);
    m_autoAssignCheck->setChecked(true);
    optionsRow->addWidget(m_squadUpdateCheck);
    optionsRow->addWidget(m_autoAssignCheck);
    optionsRow->addStretch(1);
    layout->addLayout(optionsRow);

    connect(browseButton, &QPushButton::clicked, this, &DashboardPage::chooseImportFile);
    connect(m_importButton, &QPushButton::clicked, this, &DashboardPage::startImport);
    return box;
}

QWidget *DashboardPage::buildAnalysisHeader(QWidget *parent)
{
    auto *widget = new QWidget(parent);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *row = new QHBoxLayout;
    auto *title = new QLabel(tr("Kader-Analyse"), widget);
    title->setObjectName(QStringLiteral("sectionTitle"));
    row->addWidget(title);
    row->addSpacing(24);

    row->addWidget(new QLabel(tr("Mein Verein:"), widget));
    m_clubCombo = new QComboBox(widget);
    m_clubCombo->setMinimumWidth(220);
    m_clubCombo->setMaxVisibleItems(25);
    row->addWidget(m_clubCombo);

    row->addSpacing(16);
    row->addWidget(new QLabel(tr("Analyse-Taktik:"), widget));
    m_tacticCombo = new QComboBox(widget);
    m_tacticCombo->setMinimumWidth(220);
    row->addWidget(m_tacticCombo);
    row->addStretch(1);
    layout->addLayout(row);

    m_analysisHint = new QLabel(widget);
    m_analysisHint->setWordWrap(true);
    m_analysisHint->hide();
    layout->addWidget(m_analysisHint);

    connect(m_clubCombo, &QComboBox::activated, this, [this] {
        if (m_updatingCombos)
            return;
        m_context.database().setSetting(QStringLiteral("user_club"),
                                        m_clubCombo->currentText());
        refreshAnalysis();
    });
    connect(m_tacticCombo, &QComboBox::activated, this, [this] {
        if (!m_updatingCombos)
            refreshAnalysis();
    });
    return widget;
}

QWidget *DashboardPage::buildKpiRow(QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    const auto makeTile = [&](const QString &caption, QLabel **valueOut) {
        auto *tile = new QFrame(row);
        tile->setObjectName(QStringLiteral("kpiTile"));
        auto *tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(16, 12, 16, 12);
        auto *value = new QLabel(QStringLiteral("–"), tile);
        value->setObjectName(QStringLiteral("kpiValue"));
        auto *captionLabel = new QLabel(caption, tile);
        captionLabel->setObjectName(QStringLiteral("kpiCaption"));
        tileLayout->addWidget(value);
        tileLayout->addWidget(captionLabel);
        layout->addWidget(tile, 1);
        *valueOut = value;
    };

    makeTile(tr("Spieler im Kern-Kader"), &m_kpiPlayers);
    makeTile(tr("Gesamtwert Kader"), &m_kpiTotalValue);
    makeTile(tr("Ø Spielerwert"), &m_kpiAvgValue);
    makeTile(tr("Ø Alter"), &m_kpiAvgAge);
    return row;
}

QWidget *DashboardPage::buildSquadSection(QWidget *parent)
{
    auto *widget = new QWidget(parent);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto *gridColumn = new QVBoxLayout;
    auto *gridTitle = new QLabel(tr("Positionsstärke"), widget);
    gridTitle->setObjectName(QStringLiteral("sectionTitle"));
    gridTitle->setAlignment(Qt::AlignHCenter);
    m_strengthGrid = new StrengthGridWidget(m_theme, widget);
    gridColumn->addWidget(gridTitle);
    gridColumn->addWidget(m_strengthGrid, 0, Qt::AlignHCenter | Qt::AlignTop);
    gridColumn->addStretch(1);
    layout->addLayout(gridColumn);

    auto *tableColumn = new QVBoxLayout;
    m_clubTableTitle = new QLabel(widget);
    m_clubTableTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_clubTable = new QTableWidget(widget);
    m_clubTable->setColumnCount(7);
    m_clubTable->setHorizontalHeaderLabels({tr("Name"), tr("Alter"), tr("Position"),
                                            tr("Persönlichkeit"), tr("Spielzeit"),
                                            tr("Ø-Note"), tr("Marktwert")});
    m_clubTable->verticalHeader()->setVisible(false);
    m_clubTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_clubTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clubTable->setAlternatingRowColors(true);
    m_clubTable->setSortingEnabled(true);
    m_clubTable->horizontalHeader()->setStretchLastSection(true);
    m_clubTable->setMinimumHeight(430);
    PlayerActions::attachToTableWidget(m_context, m_clubTable);
    tableColumn->addWidget(m_clubTableTitle);
    tableColumn->addWidget(m_clubTable, 1);
    layout->addLayout(tableColumn, 1);

    return widget;
}

QWidget *DashboardPage::buildSuggestionSection(QWidget *parent)
{
    auto *box = new QGroupBox(tr("🎯 Transferziele"), parent);
    auto *layout = new QVBoxLayout(box);

    auto *info = new QLabel(tr("Mögliche Verstärkungen aus deiner Scouting-Datenbank, "
                               "basierend auf den Rollen der gewählten Taktik."),
                            box);
    info->setWordWrap(true);
    layout->addWidget(info);

    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Max. Alter:"), box));
    m_ageSlider = new QSlider(Qt::Horizontal, box);
    m_ageSlider->setRange(15, 40);
    m_ageSlider->setValue(28);
    m_ageLabel = new QLabel(box);
    m_ageLabel->setMinimumWidth(70);
    filterRow->addWidget(m_ageSlider, 1);
    filterRow->addWidget(m_ageLabel);
    filterRow->addSpacing(20);
    filterRow->addWidget(new QLabel(tr("Max. Marktwert:"), box));
    m_valueSlider = new QSlider(Qt::Horizontal, box);
    m_valueSlider->setRange(0, 400); // 0.5 Mio € steps -> 0..200 Mio €
    m_valueSlider->setValue(20);     // 10 Mio €
    m_valueLabel = new QLabel(box);
    m_valueLabel->setMinimumWidth(110);
    filterRow->addWidget(m_valueSlider, 1);
    filterRow->addWidget(m_valueLabel);
    layout->addLayout(filterRow);

    m_suggestionStatus = new QLabel(box);
    m_suggestionStatus->setWordWrap(true);
    layout->addWidget(m_suggestionStatus);

    auto *cardsHost = new QWidget(box);
    m_cardsLayout = new QGridLayout(cardsHost);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(12);
    layout->addWidget(cardsHost);

    const auto updateLabels = [this] {
        m_ageLabel->setText(tr("%1 Jahre").arg(m_ageSlider->value()));
        m_valueLabel->setText(formatMillions(maxValueFilter()));
    };
    updateLabels();
    connect(m_ageSlider, &QSlider::valueChanged, this, [this, updateLabels] {
        updateLabels();
        m_suggestionDebounce->start();
    });
    connect(m_valueSlider, &QSlider::valueChanged, this, [this, updateLabels] {
        updateLabels();
        m_suggestionDebounce->start();
    });
    return box;
}

QString DashboardPage::selectedTactic() const
{
    return m_tacticCombo ? m_tacticCombo->currentText() : QString();
}

double DashboardPage::maxValueFilter() const
{
    return m_valueSlider->value() * 500'000.0;
}

void DashboardPage::refresh()
{
    refreshCombos();
    refreshAnalysis();
}

void DashboardPage::refreshCombos()
{
    m_updatingCombos = true;

    // Distinct clubs, sorted; keep the persisted selection.
    const QString savedClub = m_context.userClub();
    QSet<QString> clubSet;
    for (const Player &player : m_context.store().players()) {
        if (!player.club.isEmpty())
            clubSet.insert(player.club);
    }
    QStringList clubs(clubSet.cbegin(), clubSet.cend());
    std::sort(clubs.begin(), clubs.end());
    m_clubCombo->clear();
    m_clubCombo->addItem(QString()); // "no club selected"
    m_clubCombo->addItems(clubs);
    if (!savedClub.isEmpty())
        m_clubCombo->setCurrentText(savedClub);

    // Tactics sorted alphabetically (legacy), favorite tactic preselected.
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

    m_updatingCombos = false;
}

void DashboardPage::refreshAnalysis()
{
    const QString userClub = m_context.userClub();
    const QString tactic = selectedTactic();

    if (userClub.isEmpty() || m_context.store().isEmpty()) {
        m_analysisHint->setText(
            tr("Bitte importiere Spielerdaten und wähle oben deinen Verein, "
               "um die Kader-Analyse zu sehen."));
        m_analysisHint->show();
        m_kpiRow->hide();
        m_squadSection->hide();
        m_suggestionBox->hide();
        return;
    }
    m_analysisHint->hide();
    m_kpiRow->show();
    m_squadSection->show();
    m_suggestionBox->show();

    // Club pool in store order (mirrors the legacy list order).
    std::vector<const Player *> clubPlayers;
    for (const Player &player : m_context.store().players()) {
        if (player.club == userClub)
            clubPlayers.push_back(&player);
    }
    updateClubTable(clubPlayers);

    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    if (clubPlayers.empty() || positions.isEmpty()) {
        m_kpiRow->hide();
        m_strengthGrid->clearData();
        m_analysisHint->setText(
            tr("Für die Taktik '%1' konnte kein Kader gebildet werden — es gibt "
               "keine passenden Spieler in deinem Verein.")
                .arg(tactic));
        m_analysisHint->show();
        clearSuggestionCards();
        m_suggestionStatus->clear();
        return;
    }

    const QStringList slotOrder = m_context.definitions().tacticSlotOrder(tactic);
    const SquadResult squad = m_context.squadBuilder().calculateSquadAndSurplus(
        clubPlayers, positions, slotOrder, m_context.ratings());

    // Core squad players (XI + B-team + depth).
    std::vector<const Player *> coreSquad;
    for (const Player *player : clubPlayers) {
        if (squad.coreSquadUids.contains(player->uid))
            coreSquad.push_back(player);
    }
    if (coreSquad.empty()) {
        m_kpiRow->hide();
        m_strengthGrid->clearData();
        m_analysisHint->setText(
            tr("Für die Taktik '%1' konnte kein Kader gebildet werden — es gibt "
               "keine passenden Spieler in deinem Verein.")
                .arg(tactic));
        m_analysisHint->show();
    } else {
        updateKpis(coreSquad);
    }

    // Positional strengths: XI + B-team + best depth options per slot.
    QHash<QString, StrengthGridWidget::SlotStrength> strengths;
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        const QString &slot = it.key();
        QList<double> values;
        const XiCell xi = squad.startingXi.value(slot);
        if (xi.isFilled())
            values << xi.rating;
        const XiCell b = squad.bTeam.value(slot);
        if (b.isFilled())
            values << b.rating;
        const auto depth = squad.bestDepthOptions.value(it.value());
        for (const DepthOption &option : depth)
            values << option.rating;

        StrengthGridWidget::SlotStrength strength;
        if (!values.isEmpty()) {
            strength.min = *std::min_element(values.cbegin(), values.cend());
            strength.max = *std::max_element(values.cbegin(), values.cend());
            double sum = 0.0;
            for (const double v : values)
                sum += v;
            strength.avg = sum / values.size();
        }
        strengths.insert(slot, strength);
    }
    m_strengthGrid->setData(strengths, m_context.definitions().tacticLayouts().value(tactic));

    rebuildSuggestions();
}

void DashboardPage::updateKpis(const std::vector<const Player *> &coreSquad)
{
    double totalValue = 0.0;
    int ageSum = 0, ageCount = 0;
    for (const Player *player : coreSquad) {
        // "Not for Sale" (sentinel) counts as 0 for the sums (legacy).
        if (player->transferValue < kUnbuyableValue)
            totalValue += player->transferValue;
        if (player->age > 0) {
            ageSum += player->age;
            ++ageCount;
        }
    }
    const double avgValue = coreSquad.empty() ? 0.0 : totalValue / coreSquad.size();
    const double avgAge = ageCount > 0 ? static_cast<double>(ageSum) / ageCount : 0.0;

    m_kpiPlayers->setText(QString::number(coreSquad.size()));
    m_kpiTotalValue->setText(formatMillions(totalValue));
    m_kpiAvgValue->setText(formatMillions(avgValue));
    m_kpiAvgAge->setText(QLocale().toString(avgAge, 'f', 1));
}

void DashboardPage::updateClubTable(const std::vector<const Player *> &clubPlayers)
{
    m_clubTableTitle->setText(tr("Spieler bei %1 (%2)")
                                  .arg(m_context.userClub())
                                  .arg(clubPlayers.size()));

    m_clubTable->setSortingEnabled(false);
    m_clubTable->setRowCount(static_cast<int>(clubPlayers.size()));
    int row = 0;
    for (const Player *player : clubPlayers) {
        auto *nameItem = new QTableWidgetItem(player->name);
        nameItem->setData(Qt::UserRole, player->uid);
        m_clubTable->setItem(row, 0, nameItem);
        m_clubTable->setItem(row, 1,
                             new NumericItem(QString::number(player->age), player->age));
        m_clubTable->setItem(row, 2, new QTableWidgetItem(player->positionRaw));

        auto *personality = new QTableWidgetItem(player->personality);
        const CellStyle style = personalityCellStyle(
            m_context.definitions().personalityCategory(player->personality));
        if (style.isValid()) {
            personality->setBackground(style.background);
            personality->setForeground(style.text);
        }
        m_clubTable->setItem(row, 3, personality);

        m_clubTable->setItem(row, 4, new QTableWidgetItem(player->agreedPlayingTime));
        m_clubTable->setItem(
            row, 5,
            new NumericItem(player->averageRating > 0.0
                                ? QLocale().toString(player->averageRating, 'f', 2)
                                : QStringLiteral("–"),
                            player->averageRating));
        m_clubTable->setItem(row, 6,
                             new NumericItem(player->transferValueRaw, player->transferValue));
        ++row;
    }
    m_clubTable->setSortingEnabled(true);
    m_clubTable->resizeColumnsToContents();
}

void DashboardPage::clearSuggestionCards()
{
    while (QLayoutItem *item = m_cardsLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void DashboardPage::rebuildSuggestions()
{
    clearSuggestionCards();

    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();
    const QString tactic = selectedTactic();
    if (userClub.isEmpty() || tactic.isEmpty())
        return;

    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    QSet<QString> roleSet;
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
        roleSet.insert(it.value());
    QStringList roles(roleSet.cbegin(), roleSet.cend());
    std::sort(roles.begin(), roles.end());

    const RoleRatings &ratings = m_context.ratings();
    const QHash<QString, QString> roleNames = m_context.definitions().roleDisplayMap();
    const int maxAge = m_ageSlider->value();
    const double maxValue = maxValueFilter();

    struct Suggestion {
        QString role;
        const Player *player;
        double rating;
        double myBest;
    };
    std::vector<Suggestion> suggestions;

    const auto &players = m_context.store().players();
    for (const QString &role : roles) {
        const auto roleRatings = ratings.constFind(role);
        if (roleRatings == ratings.constEnd())
            continue;

        // Best rating for the role inside my own club.
        double myBest = 0.0;
        const Player *bestUpgrade = nullptr;
        double bestUpgradeRating = 0.0;
        for (const Player &player : players) {
            const auto ratingIt = roleRatings.value().constFind(player.uid);
            if (ratingIt == roleRatings.value().constEnd())
                continue;
            if (player.club == userClub) {
                myBest = std::max(myBest, ratingIt.value());
            }
        }
        for (const Player &player : players) {
            if (player.club == userClub || (!secondClub.isEmpty() && player.club == secondClub))
                continue;
            if (player.age <= 0 || player.age > maxAge)
                continue;
            if (player.transferValue > maxValue)
                continue;
            const auto ratingIt = roleRatings.value().constFind(player.uid);
            if (ratingIt == roleRatings.value().constEnd())
                continue;
            if (ratingIt.value() > myBest && ratingIt.value() > bestUpgradeRating) {
                bestUpgrade = &player;
                bestUpgradeRating = ratingIt.value();
            }
        }
        if (bestUpgrade)
            suggestions.push_back({role, bestUpgrade, bestUpgradeRating, myBest});
    }

    if (suggestions.empty()) {
        m_suggestionStatus->setText(
            tr("✅ Dein Kader ist gut aufgestellt! Keine klaren Verstärkungen "
               "innerhalb der Filterkriterien gefunden."));
        return;
    }
    m_suggestionStatus->clear();

    const int columns = std::min<int>(4, static_cast<int>(suggestions.size()));
    int index = 0;
    for (const Suggestion &suggestion : suggestions) {
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("suggestionCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 10, 14, 10);
        cardLayout->setSpacing(4);

        const Player *player = suggestion.player;
        const QString roleName = roleNames.value(suggestion.role, suggestion.role);
        auto *header = new QLabel(QStringLiteral("<b>%1</b><br/><span>%2</span>")
                                      .arg(player->name.toHtmlEscaped(),
                                           tr("Upgrade für <b>%1</b>").arg(roleName.toHtmlEscaped())),
                                  card);
        header->setWordWrap(true);
        cardLayout->addWidget(header);

        auto *line = new QFrame(card);
        line->setFrameShape(QFrame::HLine);
        cardLayout->addWidget(line);

        auto *body = new QLabel(
            tr("🎯 <b>Bewertung:</b> %1 %% (dein Bester: %2 %%)<br/>"
               "💰 <b>Wert:</b> %3<br/>"
               "🎂 <b>Alter:</b> %4")
                .arg(qRound(suggestion.rating))
                .arg(qRound(suggestion.myBest))
                .arg(player->transferValueRaw.toHtmlEscaped())
                .arg(player->age),
            card);
        body->setWordWrap(true);
        cardLayout->addWidget(body);

        auto *footer = new QLabel(tr("Verein: %1<br/>Positionen: %2")
                                      .arg(player->club.toHtmlEscaped(),
                                           player->positionRaw.toHtmlEscaped()),
                                  card);
        footer->setObjectName(QStringLiteral("kpiCaption"));
        footer->setWordWrap(true);
        cardLayout->addWidget(footer);

        m_cardsLayout->addWidget(card, index / columns, index % columns);
        ++index;
    }
    for (int c = 0; c < columns; ++c)
        m_cardsLayout->setColumnStretch(c, 1);
}

void DashboardPage::chooseImportFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("FM-HTML-Export auswählen"), QString(),
        tr("HTML-Dateien (*.html *.htm);;Alle Dateien (*)"));
    if (path.isEmpty())
        return;
    m_filePathEdit->setText(path);
    m_importButton->setEnabled(true);
}

void DashboardPage::startImport()
{
    if (m_importRunning)
        return;
    const QString filePath = m_filePathEdit->text();
    if (filePath.isEmpty())
        return;

    m_pendingSquadUpdate = m_squadUpdateCheck->isChecked();
    m_importRunning = true;

    runImportPipeline(m_context, this, filePath, m_autoAssignCheck->isChecked(),
                      [this](const ImportPipelineResult &result) {
                          m_importRunning = false;
                          importFinished(result);
                      });
}

void DashboardPage::importFinished(const ImportPipelineResult &result)
{
    if (!result.import.success) {
        QMessageBox::critical(
            this, tr("Import fehlgeschlagen"),
            tr("❌ Die Datei konnte nicht importiert werden.\n\n%1\n\n"
               "Mögliche Ursachen: kein <table>-Element, keine 'UID'-Spalte oder "
               "kein gültiger Football-Manager-HTML-Export.")
                .arg(result.import.error));
        return;
    }

    const QStringList summary = importSummaryLines(result);
    const QStringList warnings = importWarningLines(result);

    QMessageBox box(this);
    box.setIcon(warnings.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    box.setWindowTitle(tr("Import abgeschlossen"));
    box.setText(summary.join(QLatin1Char('\n')));
    if (!warnings.isEmpty())
        box.setDetailedText(warnings.join(QStringLiteral("\n\n")));
    box.exec();

    // --- Departure detection for full squad updates. ---
    if (m_pendingSquadUpdate && !m_context.userClub().isEmpty()) {
        const QSet<QString> affected(result.import.affectedUids.cbegin(),
                                     result.import.affectedUids.cend());
        resolveDepartures(affected);
    }
    m_pendingSquadUpdate = false;
}

void DashboardPage::resolveDepartures(const QSet<QString> &affectedUids)
{
    const QString userClub = m_context.userClub();
    const QString secondClub = m_context.secondTeamClub();

    std::vector<const Player *> missing;
    for (const Player &player : m_context.store().players()) {
        const bool atMyClub = player.club == userClub
                              || (!secondClub.isEmpty() && player.club == secondClub);
        if (atMyClub && !affectedUids.contains(player.uid))
            missing.push_back(&player);
    }
    if (missing.empty())
        return;

    DepartureDialog dialog(missing, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    std::vector<Player> updates;
    for (const DepartureChoice &choice : dialog.choices()) {
        if (!choice.left)
            continue; // stayed at the club -> no change
        const Player *player = m_context.store().findByUid(choice.uid);
        if (!player)
            continue;
        // Empty destination falls back to the free-agent default.
        const QString destination = choice.destination.isEmpty() ? freeAgentTag()
                                                                  : choice.destination;
        if (destination == player->club)
            continue;
        Player updated = *player;
        updated.club = destination;
        updates.push_back(std::move(updated));
    }
    if (updates.empty())
        return;

    if (!m_context.database().upsertPlayers(updates)) {
        QMessageBox::critical(this, tr("Abgänge"), m_context.database().errorString());
        return;
    }
    m_context.reloadFromDatabase();
    QMessageBox::information(this, tr("Abgänge"),
                             tr("%1 Spieler-Datensätze aktualisiert.").arg(updates.size()));
}

} // namespace fm
