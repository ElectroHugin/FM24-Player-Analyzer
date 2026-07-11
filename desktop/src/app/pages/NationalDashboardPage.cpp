#include "NationalDashboardPage.h"

#include "../AppContext.h"
#include "../theming/ThemeManager.h"
#include "../widgets/StrengthGridWidget.h"
#include "PageHelpers.h"
#include "core/Database.h"
#include "core/Utils.h"

#include <QCheckBox>
#include <QComboBox>
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
#include <QScrollArea>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

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

} // namespace

NationalDashboardPage::NationalDashboardPage(AppContext &context, ThemeManager &theme,
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
    layout->setSpacing(12);
    scroll->setWidget(content);

    auto *heading =
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("National-Dashboard")), content);
    layout->addWidget(heading);

    // --- Import section ---
    auto *importBox =
        new QGroupBox(tr("⬆️ Neue Spielerdaten importieren (FM-HTML-Export)"), content);
    auto *importLayout = new QVBoxLayout(importBox);
    auto *fileRow = new QHBoxLayout;
    m_filePathEdit = new QLineEdit(importBox);
    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setPlaceholderText(tr("Keine Datei ausgewählt…"));
    auto *browseButton = new QPushButton(tr("Durchsuchen…"), importBox);
    m_importButton = new QPushButton(tr("Importieren"), importBox);
    m_importButton->setEnabled(false);
    fileRow->addWidget(m_filePathEdit, 1);
    fileRow->addWidget(browseButton);
    fileRow->addWidget(m_importButton);
    importLayout->addLayout(fileRow);
    auto *optionsRow = new QHBoxLayout;
    m_replaceSquadCheck =
        new QCheckBox(tr("Nationalkader durch Spieler dieser Datei ersetzen"), importBox);
    m_replaceSquadCheck->setToolTip(
        tr("Wenn aktiviert, wird der aktuelle Nationalkader geleert und durch ALLE Spieler "
           "aus dieser HTML-Datei ersetzt — praktisch für schnelle Kader-Updates."));
    m_autoAssignCheck =
        new QCheckBox(tr("Neuen/unzugeordneten Spielern automatisch Rollen zuweisen"),
                      importBox);
    m_autoAssignCheck->setChecked(true);
    optionsRow->addWidget(m_replaceSquadCheck);
    optionsRow->addWidget(m_autoAssignCheck);
    optionsRow->addStretch(1);
    importLayout->addLayout(optionsRow);
    layout->addWidget(importBox);
    connect(browseButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("FM-HTML-Export auswählen"), QString(),
            tr("HTML-Dateien (*.html *.htm);;Alle Dateien (*)"));
        if (path.isEmpty())
            return;
        m_filePathEdit->setText(path);
        m_importButton->setEnabled(true);
    });
    connect(m_importButton, &QPushButton::clicked, this, &NationalDashboardPage::startImport);

    m_hint = new QLabel(content);
    m_hint->setWordWrap(true);
    m_hint->hide();
    layout->addWidget(m_hint);

    // --- Analysis block ---
    m_analysisWidget = new QWidget(content);
    auto *analysisLayout = new QVBoxLayout(m_analysisWidget);
    analysisLayout->setContentsMargins(0, 0, 0, 0);
    analysisLayout->setSpacing(12);

    auto *tacticRow = new QHBoxLayout;
    auto *analysisTitle = new QLabel(tr("Kader-Analyse"), m_analysisWidget);
    analysisTitle->setObjectName(QStringLiteral("sectionTitle"));
    tacticRow->addWidget(analysisTitle);
    tacticRow->addSpacing(24);
    tacticRow->addWidget(new QLabel(tr("Analyse-Taktik:"), m_analysisWidget));
    m_tacticCombo = new QComboBox(m_analysisWidget);
    m_tacticCombo->setMinimumWidth(240);
    tacticRow->addWidget(m_tacticCombo);
    tacticRow->addStretch(1);
    analysisLayout->addLayout(tacticRow);

    auto *kpiRow = new QHBoxLayout;
    const auto makeTile = [&](const QString &caption, QLabel **valueOut) {
        auto *tile = new QFrame(m_analysisWidget);
        tile->setObjectName(QStringLiteral("kpiTile"));
        auto *tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(16, 12, 16, 12);
        auto *value = new QLabel(QStringLiteral("–"), tile);
        value->setObjectName(QStringLiteral("kpiValue"));
        auto *captionLabel = new QLabel(caption, tile);
        captionLabel->setObjectName(QStringLiteral("kpiCaption"));
        tileLayout->addWidget(value);
        tileLayout->addWidget(captionLabel);
        kpiRow->addWidget(tile, 1);
        *valueOut = value;
    };
    makeTile(tr("Spieler im Kader"), &m_kpiPlayers);
    makeTile(tr("Ø Alter"), &m_kpiAvgAge);
    kpiRow->addStretch(2);
    analysisLayout->addLayout(kpiRow);

    auto *squadRow = new QHBoxLayout;
    auto *gridColumn = new QVBoxLayout;
    auto *gridTitle = new QLabel(tr("Positionsstärke"), m_analysisWidget);
    gridTitle->setObjectName(QStringLiteral("sectionTitle"));
    gridTitle->setAlignment(Qt::AlignHCenter);
    m_strengthGrid = new StrengthGridWidget(m_theme, m_analysisWidget);
    gridColumn->addWidget(gridTitle);
    gridColumn->addWidget(m_strengthGrid, 0, Qt::AlignHCenter | Qt::AlignTop);
    gridColumn->addStretch(1);
    squadRow->addLayout(gridColumn);
    auto *tableColumn = new QVBoxLayout;
    m_squadTableTitle = new QLabel(m_analysisWidget);
    m_squadTableTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_squadTable = new QTableWidget(m_analysisWidget);
    m_squadTable->setColumnCount(5);
    m_squadTable->setHorizontalHeaderLabels(
        {tr("Name"), tr("Alter"), tr("Ø-Note"), tr("Verein"), tr("Position")});
    m_squadTable->verticalHeader()->setVisible(false);
    m_squadTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_squadTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_squadTable->setAlternatingRowColors(true);
    m_squadTable->setSortingEnabled(true);
    m_squadTable->horizontalHeader()->setStretchLastSection(true);
    m_squadTable->setMinimumHeight(430);
    tableColumn->addWidget(m_squadTableTitle);
    tableColumn->addWidget(m_squadTable, 1);
    squadRow->addLayout(tableColumn, 1);
    analysisLayout->addLayout(squadRow);

    // --- Potential call-ups ---
    auto *callUpBox = new QGroupBox(tr("🎯 Mögliche Nachnominierungen"), m_analysisWidget);
    auto *callUpLayout = new QVBoxLayout(callUpBox);
    auto *callUpInfo = new QLabel(
        tr("Mögliche Verstärkungen aus dem berechtigten Spieler-Pool, die aktuell nicht im "
           "Kader stehen."),
        callUpBox);
    callUpInfo->setWordWrap(true);
    callUpLayout->addWidget(callUpInfo);
    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Max. Alter:"), callUpBox));
    m_maxAgeSlider = new QSlider(Qt::Horizontal, callUpBox);
    m_maxAgeSlider->setRange(15, 99);
    m_maxAgeLabel = new QLabel(callUpBox);
    m_maxAgeLabel->setMinimumWidth(70);
    filterRow->addWidget(m_maxAgeSlider, 1);
    filterRow->addWidget(m_maxAgeLabel);
    callUpLayout->addLayout(filterRow);
    m_callUpStatus = new QLabel(callUpBox);
    m_callUpStatus->setWordWrap(true);
    callUpLayout->addWidget(m_callUpStatus);
    auto *cardsHost = new QWidget(callUpBox);
    m_cardsLayout = new QGridLayout(cardsHost);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(12);
    callUpLayout->addWidget(cardsHost);
    analysisLayout->addWidget(callUpBox);

    layout->addWidget(m_analysisWidget);
    layout->addStretch(1);

    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuildAnalysis();
    });
    connect(m_maxAgeSlider, &QSlider::valueChanged, this, [this] {
        m_maxAgeLabel->setText(tr("%1 Jahre").arg(m_maxAgeSlider->value()));
        if (!m_updating)
            rebuildCallUps();
    });
}

void NationalDashboardPage::refresh()
{
    m_updating = true;
    const QString previous = m_tacticCombo->currentText();
    m_tacticCombo->clear();
    m_tacticCombo->addItems(favoritesFirstTactics(m_context, true));
    if (!previous.isEmpty() && m_tacticCombo->findText(previous) >= 0)
        m_tacticCombo->setCurrentText(previous);

    const int ageLimit = m_context.nationalTeamAgeLimit();
    const int sliderMax = ageLimit > 0 ? std::min(ageLimit, 99) : 99;
    m_maxAgeSlider->setMaximum(sliderMax);
    if (m_maxAgeSlider->value() > sliderMax || m_maxAgeSlider->value() == m_maxAgeSlider->minimum())
        m_maxAgeSlider->setValue(sliderMax);
    m_maxAgeLabel->setText(tr("%1 Jahre").arg(m_maxAgeSlider->value()));
    m_updating = false;

    rebuildAnalysis();
}

void NationalDashboardPage::rebuildAnalysis()
{
    const QString name = m_context.nationalTeamName();
    const QString code = m_context.nationalTeamCode();
    const int ageLimit = m_context.nationalTeamAgeLimit();

    if (name.isEmpty() || code.isEmpty() || ageLimit <= 0) {
        m_hint->setText(
            tr("⚠️ Bitte konfiguriere zuerst dein Nationalteam vollständig unter "
               "Einstellungen → Verein (Name, Länder-Code, Altersgrenze)."));
        m_hint->show();
        m_analysisWidget->hide();
        return;
    }

    std::vector<const Player *> squadPlayers;
    for (const Player &player : m_context.store().players()) {
        if (player.inNationalSquad)
            squadPlayers.push_back(&player);
    }
    if (squadPlayers.empty()) {
        m_hint->setText(tr("Noch keine Spieler im Nationalkader. Stelle ihn unter "
                           "'Kader-Auswahl' zusammen."));
        m_hint->show();
        m_analysisWidget->hide();
        return;
    }
    m_hint->hide();
    m_analysisWidget->show();

    // --- KPIs ---
    int ageSum = 0, ageCount = 0;
    for (const Player *player : squadPlayers) {
        if (player->age > 0) {
            ageSum += player->age;
            ++ageCount;
        }
    }
    m_kpiPlayers->setText(QString::number(squadPlayers.size()));
    m_kpiAvgAge->setText(ageCount > 0
                             ? QLocale().toString(static_cast<double>(ageSum) / ageCount, 'f', 1)
                             : QStringLiteral("–"));

    // --- Squad table ---
    m_squadTableTitle->setText(tr("Aktueller %1-Kader").arg(name));
    m_squadTable->setSortingEnabled(false);
    m_squadTable->setRowCount(static_cast<int>(squadPlayers.size()));
    int row = 0;
    for (const Player *player : squadPlayers) {
        m_squadTable->setItem(row, 0, new QTableWidgetItem(player->name));
        m_squadTable->setItem(row, 1,
                              new NumericItem(QString::number(player->age), player->age));
        m_squadTable->setItem(
            row, 2,
            new NumericItem(player->averageRating > 0.0
                                ? QLocale().toString(player->averageRating, 'f', 2)
                                : QStringLiteral("–"),
                            player->averageRating));
        m_squadTable->setItem(row, 3, new QTableWidgetItem(player->club));
        m_squadTable->setItem(row, 4, new QTableWidgetItem(player->positionRaw));
        ++row;
    }
    m_squadTable->setSortingEnabled(true);
    m_squadTable->resizeColumnsToContents();

    // --- Positional strengths (no APT weighting for national squads) ---
    const QString tactic = m_tacticCombo->currentText();
    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    if (!positions.isEmpty()) {
        const QStringList slotOrder = m_context.definitions().tacticSlotOrder(tactic);
        const SquadResult squad = m_context.squadBuilder().calculateSquadAndSurplus(
            squadPlayers, positions, slotOrder, m_context.ratings(), false);

        QHash<QString, StrengthGridWidget::SlotStrength> strengths;
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            QList<double> values;
            const XiCell xi = squad.startingXi.value(it.key());
            if (xi.isFilled())
                values << xi.rating;
            const XiCell b = squad.bTeam.value(it.key());
            if (b.isFilled())
                values << b.rating;
            for (const DepthOption &option : squad.bestDepthOptions.value(it.value()))
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
            strengths.insert(it.key(), strength);
        }
        m_strengthGrid->setData(strengths,
                                m_context.definitions().tacticLayouts().value(tactic));
    } else {
        m_strengthGrid->clearData();
    }

    rebuildCallUps();
}

void NationalDashboardPage::clearCards()
{
    while (QLayoutItem *item = m_cardsLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void NationalDashboardPage::rebuildCallUps()
{
    clearCards();

    const QString code = m_context.nationalTeamCode();
    const int ageLimit = m_context.nationalTeamAgeLimit();
    const QString tactic = m_tacticCombo->currentText();
    if (code.isEmpty() || tactic.isEmpty())
        return;

    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    QSet<QString> roleSet;
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
        roleSet.insert(it.value());
    QStringList roles(roleSet.cbegin(), roleSet.cend());
    std::sort(roles.begin(), roles.end());

    const RoleRatings &ratings = m_context.ratings();
    const auto roleNames = m_context.definitions().roleDisplayMap();
    const int maxAge = m_maxAgeSlider->value();

    struct Suggestion {
        QString role;
        const Player *player;
        double rating;
        double squadBest;
    };
    std::vector<Suggestion> suggestions;
    const auto &players = m_context.store().players();

    for (const QString &role : roles) {
        const auto roleRatings = ratings.constFind(role);
        if (roleRatings == ratings.constEnd())
            continue;

        double squadBest = 0.0;
        for (const Player &player : players) {
            if (!player.inNationalSquad)
                continue;
            squadBest = std::max(squadBest, roleRatings.value().value(player.uid, 0.0));
        }
        const Player *bestUpgrade = nullptr;
        double bestRating = 0.0;
        for (const Player &player : players) {
            if (player.inNationalSquad)
                continue;
            if (player.nationality != code && player.secondNationality != code)
                continue;
            if (player.age <= 0 || player.age > maxAge
                || (ageLimit < 99 && player.age > ageLimit))
                continue;
            const auto ratingIt = roleRatings.value().constFind(player.uid);
            if (ratingIt == roleRatings.value().constEnd())
                continue;
            if (ratingIt.value() > squadBest && ratingIt.value() > bestRating) {
                bestUpgrade = &player;
                bestRating = ratingIt.value();
            }
        }
        if (bestUpgrade)
            suggestions.push_back({role, bestUpgrade, bestRating, squadBest});
    }

    if (suggestions.empty()) {
        m_callUpStatus->setText(tr("✅ Dein Kader ist gut aufgestellt! Keine klaren "
                                   "Verstärkungen im berechtigten Pool gefunden."));
        return;
    }
    m_callUpStatus->clear();

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
        auto *header = new QLabel(
            QStringLiteral("<b>%1</b><br/><span>%2</span>")
                .arg(player->name.toHtmlEscaped(),
                     tr("Upgrade für <b>%1</b>").arg(roleName.toHtmlEscaped())),
            card);
        header->setWordWrap(true);
        cardLayout->addWidget(header);
        auto *line = new QFrame(card);
        line->setFrameShape(QFrame::HLine);
        cardLayout->addWidget(line);
        auto *body = new QLabel(tr("🎯 <b>Bewertung:</b> %1 %% (dein Bester: %2 %%)<br/>"
                                   "🎂 <b>Alter:</b> %3")
                                    .arg(qRound(suggestion.rating))
                                    .arg(qRound(suggestion.squadBest))
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

void NationalDashboardPage::startImport()
{
    if (m_importRunning)
        return;
    const QString filePath = m_filePathEdit->text();
    if (filePath.isEmpty())
        return;
    m_importRunning = true;
    runImportPipeline(m_context, this, filePath, m_autoAssignCheck->isChecked(),
                      [this](const ImportPipelineResult &result) {
                          m_importRunning = false;
                          importFinished(result);
                      });
}

void NationalDashboardPage::importFinished(const ImportPipelineResult &result)
{
    if (!result.import.success) {
        QMessageBox::critical(
            this, tr("Import fehlgeschlagen"),
            tr("❌ Die Datei konnte nicht importiert werden.\n\n%1").arg(result.import.error));
        return;
    }

    QStringList summary = importSummaryLines(result);

    // Optionally replace the national squad with every player from the file.
    if (m_replaceSquadCheck->isChecked()) {
        QList<int> ids;
        for (const QString &uid : result.import.affectedUids) {
            if (const Player *player = m_context.store().findByUid(uid))
                ids << player->id;
        }
        if (!m_context.database().setNationalSquadIds(ids)) {
            QMessageBox::critical(this, tr("Nationalkader"),
                                  m_context.database().errorString());
        } else {
            summary << tr("🔁 Nationalkader durch %1 Spieler aus der Datei ersetzt.")
                           .arg(ids.size());
            m_context.reloadFromDatabase();
        }
    }

    const QStringList warnings = importWarningLines(result);
    QMessageBox box(this);
    box.setIcon(warnings.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    box.setWindowTitle(tr("Import abgeschlossen"));
    box.setText(summary.join(QLatin1Char('\n')));
    if (!warnings.isEmpty())
        box.setDetailedText(warnings.join(QStringLiteral("\n\n")));
    box.exec();
}

} // namespace fm
