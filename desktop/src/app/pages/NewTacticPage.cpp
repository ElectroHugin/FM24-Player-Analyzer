#include "NewTacticPage.h"

#include "../AppContext.h"
#include "core/Constants.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

struct SlotSpec {
    QString slot;             // tactic slot key ("AML", "DMC", …)
    QString label;            // UI label ("DML/WBL")
    QStringList gamePositions; // eligible-position keys for the role list
    int column = 0;           // 0..4 on the 5-column pitch grid
};

// Pitch rows exactly as in legacy new_tactic.py (top = strikers).
const QList<QPair<QString, QList<SlotSpec>>> &pitchRows()
{
    static const QList<QPair<QString, QList<SlotSpec>>> rows = {
        {QObject::tr("Sturm"),
         {{QStringLiteral("STL"), QStringLiteral("STL"), {QStringLiteral("ST (C)")}, 1},
          {QStringLiteral("STC"), QStringLiteral("STC"), {QStringLiteral("ST (C)")}, 2},
          {QStringLiteral("STR"), QStringLiteral("STR"), {QStringLiteral("ST (C)")}, 3}}},
        {QObject::tr("Offensives Mittelfeld"),
         {{QStringLiteral("AML"), QStringLiteral("AML"), {QStringLiteral("AM (L)")}, 0},
          {QStringLiteral("AMCL"), QStringLiteral("AMCL"), {QStringLiteral("AM (C)")}, 1},
          {QStringLiteral("AMC"), QStringLiteral("AMC"), {QStringLiteral("AM (C)")}, 2},
          {QStringLiteral("AMCR"), QStringLiteral("AMCR"), {QStringLiteral("AM (C)")}, 3},
          {QStringLiteral("AMR"), QStringLiteral("AMR"), {QStringLiteral("AM (R)")}, 4}}},
        {QObject::tr("Mittelfeld"),
         {{QStringLiteral("ML"), QStringLiteral("ML"), {QStringLiteral("M (L)")}, 0},
          {QStringLiteral("MCL"), QStringLiteral("MCL"), {QStringLiteral("M (C)")}, 1},
          {QStringLiteral("MC"), QStringLiteral("MC"), {QStringLiteral("M (C)")}, 2},
          {QStringLiteral("MCR"), QStringLiteral("MCR"), {QStringLiteral("M (C)")}, 3},
          {QStringLiteral("MR"), QStringLiteral("MR"), {QStringLiteral("M (R)")}, 4}}},
        {QObject::tr("Defensives Mittelfeld"),
         {{QStringLiteral("DML"), QStringLiteral("DML/WBL"),
           {QStringLiteral("DM"), QStringLiteral("WB (L)")}, 0},
          {QStringLiteral("DMCL"), QStringLiteral("DMCL"), {QStringLiteral("DM")}, 1},
          {QStringLiteral("DMC"), QStringLiteral("DMC"), {QStringLiteral("DM")}, 2},
          {QStringLiteral("DMCR"), QStringLiteral("DMCR"), {QStringLiteral("DM")}, 3},
          {QStringLiteral("DMR"), QStringLiteral("DMR/WBR"),
           {QStringLiteral("DM"), QStringLiteral("WB (R)")}, 4}}},
        {QObject::tr("Abwehr"),
         {{QStringLiteral("DL"), QStringLiteral("DL"), {QStringLiteral("D (L)")}, 0},
          {QStringLiteral("DCL"), QStringLiteral("DCL"), {QStringLiteral("D (C)")}, 1},
          {QStringLiteral("DC"), QStringLiteral("DC"), {QStringLiteral("D (C)")}, 2},
          {QStringLiteral("DCR"), QStringLiteral("DCR"), {QStringLiteral("D (C)")}, 3},
          {QStringLiteral("DR"), QStringLiteral("DR"), {QStringLiteral("D (R)")}, 4}}},
    };
    return rows;
}

} // namespace

NewTacticPage::NewTacticPage(AppContext &context, QWidget *parent)
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

    auto *heading =
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Neue Taktik erstellen")), content);
    layout->addWidget(heading);
    auto *info = new QLabel(
        tr("Baue deine Formation auf dem Feld unten. Wähle für jede aktive Position eine "
           "Rolle — genau ein Torhüter und zehn Feldspieler."),
        content);
    info->setWordWrap(true);
    info->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(info);

    // --- 1. Naming ---
    auto *nameBox = new QGroupBox(tr("1. Taktik-Name"), content);
    auto *nameRow = new QHBoxLayout(nameBox);
    nameRow->addWidget(new QLabel(tr("Formations-Name:"), nameBox));
    m_nameEdit = new QLineEdit(nameBox);
    m_nameEdit->setPlaceholderText(tr("z. B. 'Mein Gegenpressing'"));
    m_nameEdit->setMinimumWidth(220);
    nameRow->addWidget(m_nameEdit, 2);
    nameRow->addWidget(new QLabel(tr("Formation:"), nameBox));
    m_shapeEdit = new QLineEdit(nameBox);
    m_shapeEdit->setPlaceholderText(QStringLiteral("4-2-3-1"));
    m_shapeEdit->setMaximumWidth(120);
    nameRow->addWidget(m_shapeEdit, 1);
    layout->addWidget(nameBox);
    m_fullNameLabel = new QLabel(content);
    layout->addWidget(m_fullNameLabel);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewTacticPage::updateFullName);
    connect(m_shapeEdit, &QLineEdit::textChanged, this, &NewTacticPage::updateFullName);

    // --- 2. Formation designer ---
    auto *pitchBox = new QGroupBox(tr("2. Formation gestalten"), content);
    auto *pitchLayout = new QVBoxLayout(pitchBox);
    m_countLabel = new QLabel(pitchBox);
    pitchLayout->addWidget(m_countLabel);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);
    int gridRow = 0;
    for (const auto &[stratumLabel, specs] : pitchRows()) {
        auto *stratumTitle = new QLabel(stratumLabel, pitchBox);
        stratumTitle->setAlignment(Qt::AlignHCenter);
        stratumTitle->setObjectName(QStringLiteral("kpiCaption"));
        grid->addWidget(stratumTitle, gridRow++, 0, 1, 5);
        for (const SlotSpec &spec : specs) {
            auto *cell = new QVBoxLayout;
            auto *slotLabel = new QLabel(spec.label, pitchBox);
            slotLabel->setAlignment(Qt::AlignHCenter);
            auto *combo = new QComboBox(pitchBox);
            combo->setMinimumWidth(150);
            m_slotCombos.insert(spec.slot, combo);
            cell->addWidget(slotLabel);
            cell->addWidget(combo);
            auto *cellWidget = new QWidget(pitchBox);
            cellWidget->setLayout(cell);
            grid->addWidget(cellWidget, gridRow, spec.column);
            connect(combo, &QComboBox::currentIndexChanged, this,
                    &NewTacticPage::updateFullName);
        }
        ++gridRow;
    }
    // Goalkeeper row spanning the width.
    auto *gkTitle = new QLabel(tr("Torhüter"), pitchBox);
    gkTitle->setAlignment(Qt::AlignHCenter);
    gkTitle->setObjectName(QStringLiteral("kpiCaption"));
    grid->addWidget(gkTitle, gridRow++, 0, 1, 5);
    auto *gkCombo = new QComboBox(pitchBox);
    gkCombo->setMinimumWidth(220);
    m_slotCombos.insert(QStringLiteral("GK"), gkCombo);
    grid->addWidget(gkCombo, gridRow, 1, 1, 3);
    pitchLayout->addLayout(grid);
    layout->addWidget(pitchBox);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    m_createButton = new QPushButton(tr("Neue Taktik erstellen"), content);
    m_createButton->setDefault(true);
    buttonRow->addWidget(m_createButton);
    layout->addLayout(buttonRow);
    layout->addStretch(1);
    connect(m_createButton, &QPushButton::clicked, this, &NewTacticPage::createTactic);
}

void NewTacticPage::fillSlotCombo(QComboBox *combo, const QStringList &gamePositions)
{
    const QString previous = combo->currentData().toString();
    const auto posMap = m_context.definitions().positionToRoleMapping();
    const auto roleNames = m_context.definitions().roleDisplayMap();

    QStringList roles;
    for (const QString &position : gamePositions)
        roles << posMap.value(position);
    roles.removeDuplicates();
    std::sort(roles.begin(), roles.end(), [&roleNames](const QString &a, const QString &b) {
        return roleNames.value(a, a) < roleNames.value(b, b);
    });

    combo->blockSignals(true);
    combo->clear();
    combo->addItem(tr("— Unbesetzt —"), QString());
    for (const QString &role : roles)
        combo->addItem(roleNames.value(role, role), role);
    if (!previous.isEmpty()) {
        const int index = combo->findData(previous);
        if (index >= 0)
            combo->setCurrentIndex(index);
    }
    combo->blockSignals(false);
}

void NewTacticPage::refresh()
{
    for (const auto &[stratumLabel, specs] : pitchRows()) {
        Q_UNUSED(stratumLabel);
        for (const SlotSpec &spec : specs)
            fillSlotCombo(m_slotCombos.value(spec.slot), spec.gamePositions);
    }
    // Goalkeeper: every GK/SK role (legacy).
    QComboBox *gkCombo = m_slotCombos.value(QStringLiteral("GK"));
    const QString previous = gkCombo->currentData().toString();
    const auto roleNames = m_context.definitions().roleDisplayMap();
    QStringList gkRoles = m_context.definitions().gkRoles();
    std::sort(gkRoles.begin(), gkRoles.end(), [&roleNames](const QString &a, const QString &b) {
        return roleNames.value(a, a) < roleNames.value(b, b);
    });
    gkCombo->blockSignals(true);
    gkCombo->clear();
    for (const QString &role : gkRoles)
        gkCombo->addItem(roleNames.value(role, role), role);
    if (!previous.isEmpty()) {
        const int index = gkCombo->findData(previous);
        if (index >= 0)
            gkCombo->setCurrentIndex(index);
    }
    gkCombo->blockSignals(false);

    updateFullName();
}

QString NewTacticPage::fullName() const
{
    const QString shape = m_shapeEdit->text().trimmed();
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty())
        return QString();
    return QStringLiteral("%1 %2").arg(shape, name).trimmed();
}

void NewTacticPage::updateFullName()
{
    int outfield = 0;
    for (auto it = m_slotCombos.constBegin(); it != m_slotCombos.constEnd(); ++it) {
        if (it.key() != QLatin1String("GK") && !it.value()->currentData().toString().isEmpty())
            ++outfield;
    }
    const QString name = fullName();
    m_fullNameLabel->setText(
        tr("<b>Vollständiger Taktik-Name:</b> <code>%1</code>")
            .arg(name.isEmpty() ? QStringLiteral("…") : name));
    m_countLabel->setText(outfield == 10
                              ? tr("✅ 10 Feldspieler ausgewählt.")
                              : tr("Feldspieler ausgewählt: %1 / 10").arg(outfield));
}

void NewTacticPage::createTactic()
{
    const QString name = fullName();
    if (m_nameEdit->text().trimmed().isEmpty() || m_shapeEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Neue Taktik"),
                             tr("Bitte gib sowohl einen Formations-Namen als auch eine "
                                "Formation (z. B. 4-2-3-1) an."));
        return;
    }
    if (m_context.definitions().tacticNames().contains(name)) {
        QMessageBox::warning(this, tr("Neue Taktik"),
                             tr("Eine Taktik namens '%1' existiert bereits. Bitte wähle "
                                "einen anderen Namen.")
                                 .arg(name));
        return;
    }

    QHash<QString, QString> outfield; // slot -> role
    for (auto it = m_slotCombos.constBegin(); it != m_slotCombos.constEnd(); ++it) {
        if (it.key() == QLatin1String("GK"))
            continue;
        const QString role = it.value()->currentData().toString();
        if (!role.isEmpty())
            outfield.insert(it.key(), role);
    }
    if (outfield.size() != 10) {
        QMessageBox::warning(this, tr("Neue Taktik"),
                             tr("Du musst genau 10 Feldspieler auswählen (aktuell: %1).")
                                 .arg(outfield.size()));
        return;
    }
    const QString gkRole = m_slotCombos.value(QStringLiteral("GK"))->currentData().toString();
    if (gkRole.isEmpty()) {
        QMessageBox::warning(this, tr("Neue Taktik"), tr("Bitte wähle eine Torhüter-Rolle."));
        return;
    }

    // --- tactic_roles: slot -> role (GK first, like legacy). ---
    QJsonObject tacticRoles;
    tacticRoles.insert(QStringLiteral("GK"), gkRole);
    for (auto it = outfield.constBegin(); it != outfield.constEnd(); ++it)
        tacticRoles.insert(it.key(), it.value());

    // --- tactic_layouts: stratum -> [slots] from the master position map. ---
    QJsonObject tacticLayout;
    for (auto it = outfield.constBegin(); it != outfield.constEnd(); ++it) {
        const auto posIt = masterPositionMap().constFind(it.key());
        if (posIt == masterPositionMap().constEnd())
            continue;
        const QString stratum = posIt.value().stratum;
        QJsonArray stratumSlots = tacticLayout.value(stratum).toArray();
        stratumSlots.append(it.key());
        tacticLayout.insert(stratum, stratumSlots);
    }

    QJsonObject root = m_context.definitions().root();
    QJsonObject allTactics = root.value(QLatin1String("tactic_roles")).toObject();
    allTactics.insert(name, tacticRoles);
    root.insert(QStringLiteral("tactic_roles"), allTactics);
    QJsonObject allLayouts = root.value(QLatin1String("tactic_layouts")).toObject();
    allLayouts.insert(name, tacticLayout);
    root.insert(QStringLiteral("tactic_layouts"), allLayouts);

    m_context.definitions().setRoot(root);
    if (!m_context.definitions().save()) {
        QMessageBox::critical(this, tr("Neue Taktik"),
                              m_context.definitions().errorString());
        return;
    }
    m_context.reloadConfigAndDefinitions();

    QMessageBox::information(this, tr("Neue Taktik"),
                             tr("Taktik '%1' wurde erstellt und ist jetzt überall "
                                "verfügbar.")
                                 .arg(name));
    resetForm();
}

void NewTacticPage::resetForm()
{
    m_nameEdit->clear();
    m_shapeEdit->clear();
    for (auto it = m_slotCombos.constBegin(); it != m_slotCombos.constEnd(); ++it) {
        if (it.key() != QLatin1String("GK"))
            it.value()->setCurrentIndex(0);
    }
    updateFullName();
}

} // namespace fm
