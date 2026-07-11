#include "NewRolePage.h"

#include "../AppContext.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

// Attribute groups exactly as in legacy new_role.py (some entries — e.g.
// Free Kick Taking — are not imported from FM; they simply contribute no
// value, same as in the legacy app).
const QStringList &technicalAttrs()
{
    static const QStringList attrs = {
        QStringLiteral("Corners"),        QStringLiteral("Crossing"),
        QStringLiteral("Dribbling"),      QStringLiteral("Finishing"),
        QStringLiteral("First Touch"),    QStringLiteral("Free Kick Taking"),
        QStringLiteral("Heading"),        QStringLiteral("Long Shots"),
        QStringLiteral("Long Throws"),    QStringLiteral("Marking"),
        QStringLiteral("Passing"),        QStringLiteral("Penalty Taking"),
        QStringLiteral("Tackling"),       QStringLiteral("Technique")};
    return attrs;
}

const QStringList &mentalAttrs()
{
    static const QStringList attrs = {
        QStringLiteral("Aggression"),    QStringLiteral("Anticipation"),
        QStringLiteral("Bravery"),       QStringLiteral("Composure"),
        QStringLiteral("Concentration"), QStringLiteral("Decisions"),
        QStringLiteral("Determination"), QStringLiteral("Flair"),
        QStringLiteral("Leadership"),    QStringLiteral("Off the Ball"),
        QStringLiteral("Positioning"),   QStringLiteral("Teamwork"),
        QStringLiteral("Vision"),        QStringLiteral("Work Rate")};
    return attrs;
}

const QStringList &physicalAttrs()
{
    static const QStringList attrs = {
        QStringLiteral("Acceleration"),  QStringLiteral("Agility"),
        QStringLiteral("Balance"),       QStringLiteral("Jumping Reach"),
        QStringLiteral("Natural Fitness"), QStringLiteral("Pace"),
        QStringLiteral("Stamina"),       QStringLiteral("Strength")};
    return attrs;
}

} // namespace

NewRolePage::NewRolePage(AppContext &context, QWidget *parent)
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
        new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Neue Rolle erstellen")), content);
    layout->addWidget(heading);
    auto *info = new QLabel(
        tr("Definiere eine neue Feldspieler-Rolle. Das Kürzel wird beim Tippen automatisch "
           "erzeugt. Nach dem Erstellen ist die Rolle überall in der App verfügbar."),
        content);
    info->setWordWrap(true);
    info->setObjectName(QStringLiteral("kpiCaption"));
    layout->addWidget(info);

    // --- 1. Basic info ---
    auto *basicBox = new QGroupBox(tr("1. Grunddaten"), content);
    auto *basicRow = new QHBoxLayout(basicBox);
    basicRow->addWidget(new QLabel(tr("Voller Rollenname:"), basicBox));
    m_nameEdit = new QLineEdit(basicBox);
    m_nameEdit->setPlaceholderText(tr("z. B. 'Advanced Playmaker'"));
    m_nameEdit->setMinimumWidth(240);
    basicRow->addWidget(m_nameEdit, 1);
    basicRow->addWidget(new QLabel(tr("Kategorie:"), basicBox));
    m_categoryCombo = new QComboBox(basicBox);
    // JSON keys stay English (definitions.json schema).
    m_categoryCombo->addItem(tr("Verteidigung"), QStringLiteral("Defense"));
    m_categoryCombo->addItem(tr("Mittelfeld"), QStringLiteral("Midfield"));
    m_categoryCombo->addItem(tr("Angriff"), QStringLiteral("Attack"));
    basicRow->addWidget(m_categoryCombo);
    basicRow->addWidget(new QLabel(tr("Aufgabe:"), basicBox));
    m_dutyCombo = new QComboBox(basicBox);
    for (const QString &duty : {QStringLiteral("Defend"), QStringLiteral("Support"),
                                QStringLiteral("Attack"), QStringLiteral("Automatic"),
                                QStringLiteral("Cover"), QStringLiteral("Stopper")})
        m_dutyCombo->addItem(duty, duty);
    basicRow->addWidget(m_dutyCombo);
    layout->addWidget(basicBox);

    m_shortNameLabel = new QLabel(content);
    layout->addWidget(m_shortNameLabel);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewRolePage::updateShortName);
    connect(m_dutyCombo, &QComboBox::currentIndexChanged, this, &NewRolePage::updateShortName);

    // --- 2. Position mapping ---
    auto *positionBox = new QGroupBox(tr("2. Positions-Zuordnung"), content);
    auto *positionLayout = new QVBoxLayout(positionBox);
    auto *positionHint = new QLabel(
        tr("Positionen ankreuzen, auf denen diese Rolle eingesetzt werden kann."),
        positionBox);
    positionHint->setObjectName(QStringLiteral("kpiCaption"));
    positionLayout->addWidget(positionHint);
    m_positionList = new QListWidget(positionBox);
    m_positionList->setMaximumHeight(160);
    m_positionList->setFlow(QListView::TopToBottom);
    m_positionList->setWrapping(true);
    m_positionList->setResizeMode(QListView::Adjust);
    positionLayout->addWidget(m_positionList);
    layout->addWidget(positionBox);

    // --- 3. Attributes ---
    auto *attrBox = new QGroupBox(tr("3. Schlüssel- und bevorzugte Attribute"), content);
    auto *attrLayout = new QVBoxLayout(attrBox);
    auto *attrHint = new QLabel(
        tr("'Schlüssel' (S) erhält den höchsten Multiplikator, 'Bevorzugt' (B) einen "
           "mittleren. Sind beide angekreuzt, gewinnt 'Schlüssel'."),
        attrBox);
    attrHint->setWordWrap(true);
    attrHint->setObjectName(QStringLiteral("kpiCaption"));
    attrLayout->addWidget(attrHint);

    auto *columnsRow = new QHBoxLayout;
    const auto makeColumn = [&](const QString &title, const QStringList &attrs) {
        auto *group = new QGroupBox(title, attrBox);
        auto *grid = new QGridLayout(group);
        grid->setColumnStretch(0, 1);
        grid->addWidget(new QLabel(QStringLiteral("<b>S</b>"), group), 0, 1);
        grid->addWidget(new QLabel(QStringLiteral("<b>B</b>"), group), 0, 2);
        int row = 1;
        for (const QString &attr : attrs) {
            grid->addWidget(new QLabel(attr, group), row, 0);
            auto *key = new QCheckBox(group);
            auto *pref = new QCheckBox(group);
            grid->addWidget(key, row, 1);
            grid->addWidget(pref, row, 2);
            m_attrChecks.insert(attr, {key, pref});
            ++row;
        }
        columnsRow->addWidget(group, 1);
    };
    makeColumn(tr("Technisch"), technicalAttrs());
    makeColumn(tr("Mental"), mentalAttrs());
    makeColumn(tr("Physisch"), physicalAttrs());
    attrLayout->addLayout(columnsRow);
    layout->addWidget(attrBox);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    m_createButton = new QPushButton(tr("Neue Rolle erstellen"), content);
    m_createButton->setDefault(true);
    buttonRow->addWidget(m_createButton);
    layout->addLayout(buttonRow);
    layout->addStretch(1);
    connect(m_createButton, &QPushButton::clicked, this, &NewRolePage::createRole);

    updateShortName();
}

void NewRolePage::refresh()
{
    // Keep in-progress edits; only refill the position list.
    QSet<QString> checked;
    for (int i = 0; i < m_positionList->count(); ++i) {
        if (m_positionList->item(i)->checkState() == Qt::Checked)
            checked.insert(m_positionList->item(i)->text());
    }
    m_positionList->clear();

    QStringList positions = m_context.definitions().positionToRoleMapping().keys();
    positions.removeAll(QStringLiteral("GK"));
    std::sort(positions.begin(), positions.end());
    for (const QString &position : positions) {
        auto *item = new QListWidgetItem(position, m_positionList);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(checked.contains(position) ? Qt::Checked : Qt::Unchecked);
    }
}

QString NewRolePage::shortName() const
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty())
        return QString();
    QString initials;
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &word : words)
        initials += word.at(0).toUpper();
    const QString duty = m_dutyCombo->currentData().toString();
    // Legacy: first letter of the duty, except Cover/Stopper -> two letters.
    const QString suffix = (duty == QLatin1String("Cover") || duty == QLatin1String("Stopper"))
                               ? duty.left(2)
                               : duty.left(1);
    return initials + QLatin1Char('-') + suffix;
}

void NewRolePage::updateShortName()
{
    const QString name = shortName();
    m_shortNameLabel->setText(
        tr("<b>Erzeugtes Kürzel:</b> <code>%1</code> (dient als eindeutige ID)")
            .arg(name.isEmpty() ? QStringLiteral("…") : name));
}

void NewRolePage::createRole()
{
    const QString name = m_nameEdit->text().trimmed();
    const QString abbr = shortName();
    QStringList positions;
    for (int i = 0; i < m_positionList->count(); ++i) {
        if (m_positionList->item(i)->checkState() == Qt::Checked)
            positions << m_positionList->item(i)->text();
    }

    if (name.isEmpty() || positions.isEmpty()) {
        QMessageBox::warning(this, tr("Neue Rolle"),
                             tr("Bitte gib einen Rollennamen an und wähle mindestens eine "
                                "Position."));
        return;
    }
    if (m_context.definitions().validRoles().contains(abbr)
        || m_context.definitions().rolesWithWeights().contains(abbr)) {
        QMessageBox::warning(this, tr("Neue Rolle"),
                             tr("Das Kürzel '%1' existiert bereits. Bitte wähle einen "
                                "anderen Rollennamen oder eine andere Aufgabe.")
                                 .arg(abbr));
        return;
    }

    QStringList keyAttrs, prefAttrs;
    for (auto it = m_attrChecks.constBegin(); it != m_attrChecks.constEnd(); ++it) {
        if (it.value().first->isChecked())
            keyAttrs << it.key();
        else if (it.value().second->isChecked())
            prefAttrs << it.key();
    }

    const QString displayName =
        QStringLiteral("%1 (%2)").arg(name, m_dutyCombo->currentData().toString());
    const QString category = m_categoryCombo->currentData().toString();

    // --- Write into the definitions JSON (same structure as legacy). ---
    QJsonObject root = m_context.definitions().root();

    QJsonObject playerRoles = root.value(QLatin1String("player_roles")).toObject();
    QJsonObject categoryObj = playerRoles.value(category).toObject();
    categoryObj.insert(abbr, displayName);
    playerRoles.insert(category, categoryObj);
    root.insert(QStringLiteral("player_roles"), playerRoles);

    QJsonObject weights = root.value(QLatin1String("role_specific_weights")).toObject();
    QJsonObject roleWeights;
    roleWeights.insert(QStringLiteral("key"), QJsonArray::fromStringList(keyAttrs));
    roleWeights.insert(QStringLiteral("preferable"), QJsonArray::fromStringList(prefAttrs));
    weights.insert(abbr, roleWeights);
    root.insert(QStringLiteral("role_specific_weights"), weights);

    QJsonObject posMap = root.value(QLatin1String("position_to_role_mapping")).toObject();
    for (const QString &position : positions) {
        QJsonArray roles = posMap.value(position).toArray();
        QStringList roleList;
        for (const QJsonValue &value : roles)
            roleList << value.toString();
        roleList << abbr;
        roleList.sort();
        posMap.insert(position, QJsonArray::fromStringList(roleList));
    }
    root.insert(QStringLiteral("position_to_role_mapping"), posMap);

    m_context.definitions().setRoot(root);
    if (!m_context.definitions().save()) {
        QMessageBox::critical(this, tr("Neue Rolle"),
                              m_context.definitions().errorString());
        return;
    }
    // Re-read from disk and refresh the engines/pages.
    m_context.reloadConfigAndDefinitions();

    QMessageBox::information(
        this, tr("Neue Rolle"),
        tr("Rolle '%1' wurde erstellt und ist jetzt überall verfügbar. Weise sie unter "
           "'Rollen zuweisen' Spielern zu.")
            .arg(displayName));
    resetForm();
}

void NewRolePage::resetForm()
{
    m_nameEdit->clear();
    m_categoryCombo->setCurrentIndex(0);
    m_dutyCombo->setCurrentIndex(0);
    for (int i = 0; i < m_positionList->count(); ++i)
        m_positionList->item(i)->setCheckState(Qt::Unchecked);
    for (auto it = m_attrChecks.constBegin(); it != m_attrChecks.constEnd(); ++it) {
        it.value().first->setChecked(false);
        it.value().second->setChecked(false);
    }
    updateShortName();
}

} // namespace fm
