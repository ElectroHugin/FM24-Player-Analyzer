#include "NationalBestXiPage.h"

#include "../AppContext.h"
#include "../PlayerActions.h"
#include "../widgets/NumericTableItem.h"
#include "../widgets/TacticPitchWidget.h"
#include "PageHelpers.h"
#include "core/Utils.h"

#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

NationalBestXiPage::NationalBestXiPage(AppContext &context, ThemeManager &theme,
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
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Nationale Best XI")), content);
    layout->addWidget(heading);

    auto *tacticRow = new QHBoxLayout;
    tacticRow->addWidget(new QLabel(tr("Taktik:"), content));
    m_tacticCombo = new QComboBox(content);
    m_tacticCombo->setMinimumWidth(260);
    tacticRow->addWidget(m_tacticCombo);
    tacticRow->addStretch(1);
    layout->addLayout(tacticRow);

    m_hint = new QLabel(content);
    m_hint->setWordWrap(true);
    m_hint->hide();
    layout->addWidget(m_hint);

    m_content = new QWidget(content);
    auto *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    auto *aptHint = new QLabel(
        tr("Die Spielzeit-Gewichtung (Agreed Playing Time) bleibt hier außen vor — sie "
           "gehört zum Verein und darf die Nationalelf-Auswahl nicht verzerren."),
        m_content);
    aptHint->setWordWrap(true);
    aptHint->setObjectName(QStringLiteral("kpiCaption"));
    contentLayout->addWidget(aptHint);

    auto *pitchRow = new QHBoxLayout;
    auto *xiColumn = new QVBoxLayout;
    auto *xiTitle = new QLabel(tr("Startelf"), m_content);
    xiTitle->setObjectName(QStringLiteral("sectionTitle"));
    xiTitle->setAlignment(Qt::AlignHCenter);
    m_xiPitch = new TacticPitchWidget(m_theme, m_content);
    xiColumn->addWidget(xiTitle);
    xiColumn->addWidget(m_xiPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
    auto *bColumn = new QVBoxLayout;
    auto *bTitle = new QLabel(tr("B-Team"), m_content);
    bTitle->setObjectName(QStringLiteral("sectionTitle"));
    bTitle->setAlignment(Qt::AlignHCenter);
    m_bTeamPitch = new TacticPitchWidget(m_theme, m_content);
    bColumn->addWidget(bTitle);
    bColumn->addWidget(m_bTeamPitch, 0, Qt::AlignTop | Qt::AlignHCenter);
    pitchRow->addLayout(xiColumn, 1);
    pitchRow->addLayout(bColumn, 1);
    contentLayout->addLayout(pitchRow);

    m_depthBox = new QGroupBox(tr("Weitere Kader-Tiefe"), m_content);
    auto *depthLayout = new QVBoxLayout(m_depthBox);
    m_depthEmpty = new QLabel(m_depthBox);
    m_depthEmpty->setWordWrap(true);
    m_depthEmpty->setObjectName(QStringLiteral("kpiCaption"));
    depthLayout->addWidget(m_depthEmpty);
    m_depthTable = new QTableWidget(m_depthBox);
    m_depthTable->setColumnCount(4);
    m_depthTable->setHorizontalHeaderLabels(
        {tr("Rolle"), tr("Name"), tr("Alter"), tr("DWRS")});
    m_depthTable->verticalHeader()->setVisible(false);
    m_depthTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_depthTable->setAlternatingRowColors(true);
    m_depthTable->setSortingEnabled(true);
    m_depthTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_depthTable->horizontalHeader()->setStretchLastSection(true);
    m_depthTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_depthTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    depthLayout->addWidget(m_depthTable);
    contentLayout->addWidget(m_depthBox);

    layout->addWidget(m_content, 1);
    layout->addStretch(1);

    PlayerActions::attachToTableWidget(m_context, m_depthTable, 1);

    connect(m_tacticCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            rebuild();
    });

    for (TacticPitchWidget *pitch : {m_xiPitch, m_bTeamPitch}) {
        connect(pitch, &TacticPitchWidget::playerDoubleClicked, this,
                [this](const QString &uid) { PlayerActions::openProfile(m_context, uid); });
        connect(pitch, &TacticPitchWidget::playerContextMenuRequested, this,
                [this](const QString &uid, const QPoint &globalPos) {
                    PlayerActions::showContextMenu(m_context, this, uid, globalPos);
                });
    }
}

void NationalBestXiPage::refresh()
{
    m_updating = true;
    const QString previous = m_tacticCombo->currentText();
    m_tacticCombo->clear();
    m_tacticCombo->addItems(favoritesFirstTactics(m_context, true));
    if (!previous.isEmpty() && m_tacticCombo->findText(previous) >= 0)
        m_tacticCombo->setCurrentText(previous);
    m_updating = false;
    rebuild();
}

void NationalBestXiPage::rebuild()
{
    std::vector<const Player *> squadPlayers;
    for (const Player &player : m_context.store().players()) {
        if (player.inNationalSquad)
            squadPlayers.push_back(&player);
    }
    const QString tactic = m_tacticCombo->currentText();
    if (squadPlayers.empty() || tactic.isEmpty()) {
        m_hint->setText(tr("Noch keine Spieler im Nationalkader. Stelle ihn unter "
                           "'Kader-Auswahl' zusammen."));
        m_hint->show();
        m_content->hide();
        return;
    }
    m_hint->hide();
    m_content->show();

    const auto positions = m_context.definitions().tacticRoles().value(tactic);
    const QStringList slotOrder = m_context.definitions().tacticSlotOrder(tactic);
    const auto layout = m_context.definitions().tacticLayouts().value(tactic);
    const auto roleNames = m_context.definitions().roleDisplayMap();

    // apply_apt_weight=false — national selection ignores club playing time.
    const SquadResult squad = m_context.squadBuilder().calculateSquadAndSurplus(
        squadPlayers, positions, slotOrder, m_context.ratings(), false);

    m_xiPitch->setTeam(squad.startingXi, positions, layout, roleNames, false);
    m_bTeamPitch->setTeam(squad.bTeam, positions, layout, roleNames, false);

    fillDepthTable(squad.bestDepthOptions);
}

void NationalBestXiPage::fillDepthTable(const QHash<QString, QList<DepthOption>> &options)
{
    const auto roleNames = m_context.definitions().roleDisplayMap();

    if (options.isEmpty()) {
        m_depthEmpty->setText(tr("Keine weiteren Spieler im Kader als Tiefen-Optionen für "
                                 "diese Taktik geeignet."));
        m_depthEmpty->setVisible(true);
        m_depthTable->setVisible(false);
        m_depthTable->setRowCount(0);
        return;
    }
    m_depthEmpty->setVisible(false);
    m_depthTable->setVisible(true);

    // One row per (role, player) option, roles in natural pitch order.
    QStringList roles = options.keys();
    roles = m_context.definitions().sortRolesNaturally(roles);
    int rowCount = 0;
    for (const QString &role : roles)
        rowCount += options.value(role).size();

    m_depthTable->setSortingEnabled(false);
    m_depthTable->clearContents();
    m_depthTable->setRowCount(rowCount);
    int row = 0;
    for (const QString &role : roles) {
        for (const DepthOption &option : options.value(role)) {
            int column = 0;
            m_depthTable->setItem(row, column++,
                                  new QTableWidgetItem(roleNames.value(role, role)));

            auto *nameItem = new QTableWidgetItem(option.name);
            nameItem->setData(Qt::UserRole, option.playerUid);
            m_depthTable->setItem(row, column++, nameItem);

            m_depthTable->setItem(row, column++,
                                  new NumericItem(QString::number(option.age), option.age));

            auto *dwrsItem =
                new NumericItem(QStringLiteral("%1%").arg(qRound(option.rating)), option.rating);
            dwrsItem->setTextAlignment(Qt::AlignCenter);
            const CellStyle style = dwrsCellStyle(option.rating);
            if (style.isValid()) {
                dwrsItem->setBackground(style.background);
                dwrsItem->setForeground(style.text);
            }
            m_depthTable->setItem(row, column++, dwrsItem);
            ++row;
        }
    }
    m_depthTable->setSortingEnabled(true);
    m_depthTable->resizeColumnsToContents();
    m_depthTable->horizontalHeader()->setStretchLastSection(true);

    // Size the table to its content so no inner scrollbar and no dead space
    // appears between it and the pitches above.
    m_depthTable->resizeRowsToContents();
    int height = m_depthTable->horizontalHeader()->height() + 2 * m_depthTable->frameWidth();
    for (int r = 0; r < rowCount; ++r)
        height += m_depthTable->rowHeight(r);
    m_depthTable->setFixedHeight(height);
}

} // namespace fm
