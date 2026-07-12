#include "SettingsPage.h"

#include "../AppContext.h"
#include "../MigrationWizard.h"
#include "../theming/ThemeManager.h"
#include "../theming/ThemePresets.h"
#include "core/Constants.h"
#include "core/Utils.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QSet>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace fm {

namespace {

const QStringList &weightCategoryOrder()
{
    static const QStringList order = {
        QStringLiteral("Extremely Important"), QStringLiteral("Important"),
        QStringLiteral("Good"), QStringLiteral("Decent"), QStringLiteral("Almost Irrelevant")};
    return order;
}

const QStringList &gkWeightCategoryOrder()
{
    static const QStringList order = {
        QStringLiteral("Top Importance"), QStringLiteral("High Importance"),
        QStringLiteral("Medium Importance"), QStringLiteral("Key"),
        QStringLiteral("Preferable"), QStringLiteral("Other")};
    return order;
}

// Color swatch button style with a readable label on top of the color.
QString colorButtonStyle(const QColor &color)
{
    const QString textColor =
        contrastRatio(color, QColorConstants::White) >= 3.0 ? QStringLiteral("#ffffff")
                                                            : QStringLiteral("#000000");
    return QStringLiteral("background-color:%1; color:%2; border:1px solid rgba(128,128,128,0.5);"
                          " border-radius:6px; padding:5px;")
        .arg(color.name(), textColor);
}

QWidget *wrapScrollable(QWidget *content)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

} // namespace

SettingsPage::SettingsPage(AppContext &context, ThemeManager &theme, QWidget *parent)
    : PageBase(context, parent)
    , m_theme(theme)
{
    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr("Einstellungen")), this);
    layout->addWidget(heading);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildClubTab(), tr("Verein"));
    tabs->addTab(buildWeightsTab(), tr("DWRS-Gewichte"));
    tabs->addTab(buildThresholdsTab(), tr("Schwellenwerte"));
    tabs->addTab(buildThemeTab(), tr("Design"));
    tabs->addTab(buildDatabaseTab(), tr("Datenbank"));
    layout->addWidget(tabs, 1);

    auto *saveRow = new QHBoxLayout;
    saveRow->addStretch(1);
    auto *saveButton = new QPushButton(tr("Alle Einstellungen speichern"), this);
    saveButton->setDefault(true);
    saveRow->addWidget(saveButton);
    layout->addLayout(saveRow);
    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::saveAll);

    refresh();
}

QWidget *SettingsPage::buildClubTab()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);

    auto *versionGroup = new QGroupBox(tr("Football-Manager-Version"), content);
    auto *versionForm = new QFormLayout(versionGroup);
    m_fmVersionCombo = new QComboBox;
    m_fmVersionCombo->setMinimumWidth(260);
    for (const FmDataVersion &version : fmDataVersions()) {
        m_fmVersionCombo->addItem(version.supported
                                      ? version.displayName
                                      : tr("%1 (noch nicht unterstützt)")
                                            .arg(version.displayName),
                                  version.id);
        if (!version.supported)
            m_fmVersionCombo->setItemData(m_fmVersionCombo->count() - 1, false,
                                          Qt::UserRole - 1); // disable row
    }
    versionForm->addRow(tr("Version für den Import:"), m_fmVersionCombo);
    auto *versionHint = new QLabel(
        tr("Bestimmt, wie die Spielerattribute aus dem HTML-Export gelesen werden. "
           "Aktuell wird Football Manager 2024 unterstützt; weitere Versionen können "
           "später ergänzt werden."),
        versionGroup);
    versionHint->setWordWrap(true);
    versionHint->setObjectName(QStringLiteral("kpiCaption"));
    versionForm->addRow(versionHint);
    layout->addWidget(versionGroup);

    auto *clubGroup = new QGroupBox(tr("Vereins-Zuordnung"), content);
    auto *clubForm = new QFormLayout(clubGroup);
    m_userClubCombo = new QComboBox;
    m_userClubCombo->setMinimumWidth(260);
    m_secondClubCombo = new QComboBox;
    m_secondClubCombo->setMinimumWidth(260);
    m_clubCountryEdit = new QLineEdit;
    m_clubCountryEdit->setMaxLength(3);
    m_clubCountryEdit->setMaximumWidth(80);
    m_clubCountryEdit->setPlaceholderText(QStringLiteral("GER"));
    m_fullClubNameEdit = new QLineEdit;
    m_fullClubNameEdit->setMinimumWidth(260);
    m_fullClubNameEdit->setPlaceholderText(tr("z. B. 'FC Bayern München' (für den Header)"));
    m_stadiumEdit = new QLineEdit;
    m_stadiumEdit->setMinimumWidth(260);
    m_stadiumEdit->setPlaceholderText(tr("z. B. 'Allianz Arena'"));
    clubForm->addRow(tr("Mein Verein:"), m_userClubCombo);
    clubForm->addRow(tr("Zweitteam:"), m_secondClubCombo);
    clubForm->addRow(tr("Voller Vereinsname (Anzeige):"), m_fullClubNameEdit);
    clubForm->addRow(tr("Stadion:"), m_stadiumEdit);
    clubForm->addRow(tr("Vereins-Land (3-Buchstaben-Code):"), m_clubCountryEdit);

    // Logo (PNG) shown in the header — applied immediately, per database.
    m_logoPreview = new QLabel;
    m_logoPreview->setFixedSize(120, 48);
    m_logoPreview->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *logoButtons = new QHBoxLayout;
    auto *chooseLogoButton = new QPushButton(tr("Logo wählen (PNG)…"));
    m_logoRemoveButton = new QPushButton(tr("Entfernen"));
    logoButtons->addWidget(m_logoPreview, 1);
    logoButtons->addWidget(chooseLogoButton);
    logoButtons->addWidget(m_logoRemoveButton);
    auto *logoRow = new QWidget;
    logoRow->setLayout(logoButtons);
    clubForm->addRow(tr("Vereinslogo:"), logoRow);
    connect(chooseLogoButton, &QPushButton::clicked, this, &SettingsPage::chooseLogo);
    connect(m_logoRemoveButton, &QPushButton::clicked, this, &SettingsPage::removeLogo);
    layout->addWidget(clubGroup);

    auto *tacticGroup = new QGroupBox(tr("Lieblings-Taktiken"), content);
    auto *tacticForm = new QFormLayout(tacticGroup);
    m_favTactic1Combo = new QComboBox;
    m_favTactic1Combo->setMinimumWidth(260);
    m_favTactic2Combo = new QComboBox;
    m_favTactic2Combo->setMinimumWidth(260);
    tacticForm->addRow(tr("Primäre Taktik:"), m_favTactic1Combo);
    tacticForm->addRow(tr("Sekundäre Taktik:"), m_favTactic2Combo);
    layout->addWidget(tacticGroup);

    auto *natGroup = new QGroupBox(tr("Nationalteam"), content);
    auto *natForm = new QFormLayout(natGroup);
    m_natNameEdit = new QLineEdit;
    m_natNameEdit->setMinimumWidth(260);
    m_natNameEdit->setPlaceholderText(tr("z. B. Deutschland U21"));
    m_natCodeEdit = new QLineEdit;
    m_natCodeEdit->setMaxLength(3);
    m_natCodeEdit->setMaximumWidth(80);
    m_natCodeEdit->setPlaceholderText(QStringLiteral("GER"));
    m_natAgeSpin = new QSpinBox;
    m_natAgeSpin->setRange(15, 99);
    m_natAgeSpin->setValue(99);
    m_natAgeSpin->setToolTip(tr("99 = keine Altersgrenze (A-Nationalteam); z. B. 21 für U21."));
    m_natFav1Combo = new QComboBox;
    m_natFav1Combo->setMinimumWidth(260);
    m_natFav2Combo = new QComboBox;
    m_natFav2Combo->setMinimumWidth(260);
    natForm->addRow(tr("Team-Name:"), m_natNameEdit);
    natForm->addRow(tr("Länder-Code (3 Buchstaben):"), m_natCodeEdit);
    natForm->addRow(tr("Altersgrenze (99 = keine):"), m_natAgeSpin);
    natForm->addRow(tr("Primäre National-Taktik:"), m_natFav1Combo);
    natForm->addRow(tr("Sekundäre National-Taktik:"), m_natFav2Combo);
    layout->addWidget(natGroup);

    auto *hint = new QLabel(
        tr("Diese Einstellungen gelten pro Datenbank (Spielstand). Das Vereins-Land "
           "steuert den Inland-Filter der Squad Matrix; die Lieblings-Taktiken werden "
           "auf Dashboard, Squad Matrix und Transfers vorausgewählt. Die "
           "Nationalteam-Angaben aktivieren die National-Seiten (Kader-Auswahl, "
           "National-Dashboard, …)."),
        content);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);

    return wrapScrollable(content);
}

void SettingsPage::chooseLogo()
{
    const QString source = QFileDialog::getOpenFileName(
        this, tr("Vereinslogo wählen"), QString(),
        tr("Bilder (*.png *.jpg *.jpeg *.bmp);;Alle Dateien (*)"));
    if (source.isEmpty())
        return;

    QPixmap pixmap;
    if (!pixmap.load(source)) {
        QMessageBox::warning(this, tr("Vereinslogo"),
                             tr("Die Datei konnte nicht als Bild geladen werden."));
        return;
    }

    // Copy into the data folder's assets dir under a database-specific name so
    // each save keeps its own logo and the original file can move freely.
    const QString assetsDir = m_context.paths().assetsDir();
    QDir().mkpath(assetsDir);
    const QString dest = QDir(assetsDir).filePath(
        QStringLiteral("logo_%1.png").arg(m_context.currentDbName()));
    QFile::remove(dest);
    if (!pixmap.save(dest, "PNG")) {
        QMessageBox::warning(this, tr("Vereinslogo"),
                             tr("Das Logo konnte nicht gespeichert werden."));
        return;
    }

    m_context.database().setSetting(QStringLiteral("club_logo_path"), dest);
    updateLogoPreview();
    m_context.notifySettingsChanged(); // refreshes the header
}

void SettingsPage::removeLogo()
{
    const QString path = m_context.database().setting(QStringLiteral("club_logo_path"));
    if (!path.isEmpty())
        QFile::remove(path);
    m_context.database().removeSetting(QStringLiteral("club_logo_path"));
    updateLogoPreview();
    m_context.notifySettingsChanged();
}

void SettingsPage::updateLogoPreview()
{
    const QString path = m_context.database().setting(QStringLiteral("club_logo_path"));
    QPixmap pixmap;
    if (!path.isEmpty() && pixmap.load(path)) {
        m_logoPreview->setPixmap(
            pixmap.scaled(m_logoPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_logoRemoveButton->setEnabled(true);
    } else {
        m_logoPreview->setPixmap(QPixmap());
        m_logoPreview->setText(tr("(kein Logo)"));
        m_logoPreview->setStyleSheet(QStringLiteral("color: rgba(128,128,128,0.8);"));
        m_logoRemoveButton->setEnabled(false);
    }
}

QWidget *SettingsPage::buildWeightsTab()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);

    const auto makeSpin = [this](double max = 20.0) {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(0.0, max);
        spin->setDecimals(2);
        spin->setSingleStep(0.1);
        return spin;
    };

    auto *fieldGroup = new QGroupBox(tr("Feldspieler-Kategorien"), content);
    auto *fieldForm = new QFormLayout(fieldGroup);
    for (const QString &category : weightCategoryOrder()) {
        auto *spin = makeSpin();
        m_weightSpins.insert(category, spin);
        fieldForm->addRow(category + QStringLiteral(":"), spin);
    }
    layout->addWidget(fieldGroup);

    auto *gkGroup = new QGroupBox(tr("Torwart-Kategorien"), content);
    auto *gkForm = new QFormLayout(gkGroup);
    for (const QString &category : gkWeightCategoryOrder()) {
        auto *spin = makeSpin();
        m_gkWeightSpins.insert(category, spin);
        gkForm->addRow(category + QStringLiteral(":"), spin);
    }
    layout->addWidget(gkGroup);

    auto *multGroup = new QGroupBox(tr("Rollen-Multiplikatoren"), content);
    auto *multForm = new QFormLayout(multGroup);
    m_keyMultSpin = makeSpin();
    m_prefMultSpin = makeSpin();
    multForm->addRow(tr("Schlüssel-Attribute:"), m_keyMultSpin);
    multForm->addRow(tr("Bevorzugte Attribute:"), m_prefMultSpin);
    layout->addWidget(multGroup);

    auto *aptGroup = new QGroupBox(tr("Spielzeit-Gewichte (Agreed Playing Time)"), content);
    auto *aptForm = new QFormLayout(aptGroup);
    QStringList allApt = fieldPlayerAptOptions() + gkAptOptions();
    allApt.removeDuplicates();
    for (const QString &apt : allApt) {
        if (apt == QLatin1String("None"))
            continue;
        auto *spin = makeSpin(5.0);
        m_aptSpins.insert(apt, spin);
        aptForm->addRow(apt + QStringLiteral(":"), spin);
    }
    layout->addWidget(aptGroup);
    layout->addStretch(1);

    return wrapScrollable(content);
}

QWidget *SettingsPage::buildThresholdsTab()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);

    auto *ageGroup = new QGroupBox(tr("Jugend-Altersgrenzen"), content);
    auto *ageForm = new QFormLayout(ageGroup);
    m_outfielderAgeSpin = new QSpinBox;
    m_outfielderAgeSpin->setRange(15, 30);
    m_goalkeeperAgeSpin = new QSpinBox;
    m_goalkeeperAgeSpin->setRange(15, 35);
    ageForm->addRow(tr("Feldspieler bis Alter:"), m_outfielderAgeSpin);
    ageForm->addRow(tr("Torhüter bis Alter:"), m_goalkeeperAgeSpin);
    layout->addWidget(ageGroup);

    auto *squadGroup = new QGroupBox(tr("Kader-Management"), content);
    auto *squadForm = new QFormLayout(squadGroup);
    m_naturalPosSpin = new QDoubleSpinBox;
    m_naturalPosSpin->setRange(1.0, 2.0);
    m_naturalPosSpin->setDecimals(2);
    m_naturalPosSpin->setSingleStep(0.01);
    m_maxDepthRolesSpin = new QSpinBox;
    m_maxDepthRolesSpin->setRange(1, 10);
    m_minLoanTalentSpin = new QSpinBox;
    m_minLoanTalentSpin->setRange(0, 200);
    m_minLoanAgeSpin = new QSpinBox;
    m_minLoanAgeSpin->setRange(14, 25);
    m_youthLoanOverAgeSpin = new QSpinBox;
    m_youthLoanOverAgeSpin->setRange(14, 25);
    squadForm->addRow(tr("Bonus natürliche Position:"), m_naturalPosSpin);
    squadForm->addRow(tr("Max. Rollen pro Depth-Spieler:"), m_maxDepthRolesSpin);
    squadForm->addRow(tr("Min. Talent-Score für Leihe:"), m_minLoanTalentSpin);
    squadForm->addRow(tr("Mindestalter für Leihe:"), m_minLoanAgeSpin);
    squadForm->addRow(tr("Jugendelf-Leihe erst über Alter:"), m_youthLoanOverAgeSpin);
    layout->addWidget(squadGroup);

    auto *gapGroup = new QGroupBox(tr("Gap-Analyse"), content);
    auto *gapForm = new QFormLayout(gapGroup);
    const auto makeGapSpin = [] {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(0.0, 50.0);
        spin->setDecimals(1);
        return spin;
    };
    m_displacementSpin = makeGapSpin();
    m_dropoffSpin = makeGapSpin();
    m_wrongSideSpin = makeGapSpin();
    gapForm->addRow(tr("Displacement-Schwelle:"), m_displacementSpin);
    gapForm->addRow(tr("Dropoff-Schwelle:"), m_dropoffSpin);
    gapForm->addRow(tr("Falsche-Seite-Malus:"), m_wrongSideSpin);
    layout->addWidget(gapGroup);
    layout->addStretch(1);

    return wrapScrollable(content);
}

QWidget *SettingsPage::buildThemeTab()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);

    // --- Preset + mode ---
    auto *topGroup = new QGroupBox(tr("Farbschema"), content);
    auto *topForm = new QFormLayout(topGroup);
    m_presetCombo = new QComboBox;
    m_presetCombo->setMinimumWidth(280);
    m_presetCombo->setMaxVisibleItems(20);
    for (const ClubTheme &preset : clubThemes())
        m_presetCombo->addItem(preset.displayName, preset.id);
    m_presetCombo->addItem(tr("Benutzerdefiniert"), QStringLiteral("custom"));
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem(tr("Nacht (dunkel)"), QStringLiteral("night"));
    m_modeCombo->addItem(tr("Tag (hell)"), QStringLiteral("day"));
    topForm->addRow(tr("Vereins-/Farbvorlage:"), m_presetCombo);
    topForm->addRow(tr("Aktiver Modus:"), m_modeCombo);
    auto *presetHint = new QLabel(
        tr("Eine Vorlage setzt eine ruhige Anthrazit-Basis und nur die Vereinsfarben als "
           "Akzent (Kopfzeile, Buttons, Interaktion). Farben unten lassen sich danach frei "
           "anpassen — das schaltet auf 'Benutzerdefiniert'."),
        topGroup);
    presetHint->setWordWrap(true);
    presetHint->setObjectName(QStringLiteral("kpiCaption"));
    topForm->addRow(presetHint);
    layout->addWidget(topGroup);

    connect(m_presetCombo, &QComboBox::activated, this, [this] {
        const QString id = m_presetCombo->currentData().toString();
        if (id == QLatin1String("custom"))
            return;
        applyPresetToPending(id);
    });

    // --- Per-mode color roles ---
    const QList<QPair<QString, QString>> colorKeys = {
        {QStringLiteral("background_color"), tr("Hintergrund")},
        {QStringLiteral("surface_color"), tr("Panel / Karten")},
        {QStringLiteral("sidebar_color"), tr("Sidebar")},
        {QStringLiteral("header_color"), tr("Kopfzeile")},
        {QStringLiteral("primary_color"), tr("Primär / Buttons")},
        {QStringLiteral("interactive_color"), tr("Interaktion (Tabs, Slider)")},
        {QStringLiteral("text_color"), tr("Text")},
    };

    for (const QString &mode : {QStringLiteral("night"), QStringLiteral("day")}) {
        auto *group = new QGroupBox(mode == QLatin1String("night") ? tr("Nacht-Farben")
                                                                   : tr("Tag-Farben"),
                                    content);
        auto *form = new QFormLayout(group);
        for (const auto &[key, label] : colorKeys) {
            const QString fullKey = mode + QLatin1Char('_') + key;
            auto *button = new QPushButton;
            button->setFixedWidth(150);
            m_colorButtons.insert(fullKey, button);
            connect(button, &QPushButton::clicked, this, [this, fullKey, button] {
                const QColor initial(m_pendingColors.value(fullKey));
                const QColor picked =
                    QColorDialog::getColor(initial, this, tr("Farbe wählen"));
                if (!picked.isValid())
                    return;
                m_pendingColors.insert(fullKey, picked.name());
                button->setText(picked.name());
                button->setStyleSheet(colorButtonStyle(picked));
                markCustomPreset();
                updateContrastWarning();
            });
            form->addRow(label + QStringLiteral(":"), button);
        }
        layout->addWidget(group);
    }

    m_contrastLabel = new QLabel(content);
    m_contrastLabel->setWordWrap(true);
    layout->addWidget(m_contrastLabel);
    layout->addStretch(1);

    return wrapScrollable(content);
}

void SettingsPage::applyPresetToPending(const QString &presetId)
{
    const QHash<QString, QString> preset = presetThemeSettings(presetId);
    for (auto it = m_colorButtons.begin(); it != m_colorButtons.end(); ++it) {
        const QString value = preset.value(it.key());
        if (value.isEmpty())
            continue;
        m_pendingColors.insert(it.key(), value);
        it.value()->setText(value);
        it.value()->setStyleSheet(colorButtonStyle(QColor(value)));
    }
    m_pendingPreset = presetId;
    updateContrastWarning();
}

void SettingsPage::markCustomPreset()
{
    m_pendingPreset = QStringLiteral("custom");
    const int index = m_presetCombo->findData(QStringLiteral("custom"));
    if (index >= 0) {
        QSignalBlocker blocker(m_presetCombo);
        m_presetCombo->setCurrentIndex(index);
    }
}

QWidget *SettingsPage::buildDatabaseTab()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);

    auto *selectGroup = new QGroupBox(tr("Aktive Datenbank"), content);
    auto *selectLayout = new QHBoxLayout(selectGroup);
    m_dbCombo = new QComboBox;
    auto *switchButton = new QPushButton(tr("Wechseln"));
    selectLayout->addWidget(m_dbCombo, 1);
    selectLayout->addWidget(switchButton);
    layout->addWidget(selectGroup);

    connect(switchButton, &QPushButton::clicked, this, [this] {
        const QString name = m_dbCombo->currentText();
        if (name.isEmpty() || name == m_context.currentDbName())
            return;
        QString error;
        if (!m_context.openDatabase(name, &error))
            QMessageBox::critical(this, tr("Datenbank"), error);
    });

    auto *actionsRow = new QHBoxLayout;
    auto *createButton = new QPushButton(tr("Neue Datenbank anlegen…"), content);
    auto *migrateButton = new QPushButton(tr("Legacy-Datenbank importieren…"), content);
    actionsRow->addWidget(createButton);
    actionsRow->addWidget(migrateButton);
    actionsRow->addStretch(1);
    layout->addLayout(actionsRow);

    connect(createButton, &QPushButton::clicked, this, [this] {
        const QString name = QInputDialog::getText(this, tr("Neue Datenbank"),
                                                   tr("Name (ohne .db):"));
        if (name.trimmed().isEmpty())
            return;
        QString error;
        if (!m_context.openDatabase(name.trimmed(), &error))
            QMessageBox::critical(this, tr("Datenbank"), error);
        refresh();
    });
    connect(migrateButton, &QPushButton::clicked, this, [this] {
        MigrationWizard wizard(m_context, this);
        wizard.exec();
        refresh();
        if (!wizard.migratedDbName().isEmpty()
            && QMessageBox::question(this, tr("Import"),
                                     tr("Importierte Datenbank '%1' jetzt aktivieren?")
                                         .arg(wizard.migratedDbName()))
                   == QMessageBox::Yes) {
            QString error;
            if (!m_context.openDatabase(wizard.migratedDbName(), &error))
                QMessageBox::critical(this, tr("Datenbank"), error);
            refresh();
        }
    });

    auto *pathInfo = new QLabel(content);
    pathInfo->setWordWrap(true);
    pathInfo->setText(tr("Datenordner: %1\n(Änderbar über bootstrap.ini; "
                         "Verschiebe-Assistent folgt in einem späteren Meilenstein.)")
                          .arg(QDir::toNativeSeparators(m_context.paths().dataDir())));
    layout->addWidget(pathInfo);
    layout->addStretch(1);

    return wrapScrollable(content);
}

void SettingsPage::refresh()
{
    AppConfig &config = m_context.config();

    // --- Club tab ---
    const int versionIndex = m_fmVersionCombo->findData(m_context.fmVersionId());
    m_fmVersionCombo->setCurrentIndex(versionIndex >= 0 ? versionIndex : 0);

    QSet<QString> clubSet;
    for (const Player &player : m_context.store().players()) {
        if (!player.club.isEmpty())
            clubSet.insert(player.club);
    }
    QStringList clubs(clubSet.cbegin(), clubSet.cend());
    std::sort(clubs.begin(), clubs.end());
    const auto fillClubCombo = [&clubs](QComboBox *combo, const QString &current) {
        combo->clear();
        combo->addItem(QString());
        combo->addItems(clubs);
        if (!current.isEmpty())
            combo->setCurrentText(current);
    };
    fillClubCombo(m_userClubCombo, m_context.userClub());
    fillClubCombo(m_secondClubCombo, m_context.secondTeamClub());
    m_clubCountryEdit->setText(
        m_context.database().setting(QStringLiteral("club_country_code")));
    m_fullClubNameEdit->setText(
        m_context.database().setting(QStringLiteral("full_club_name")));
    m_stadiumEdit->setText(m_context.database().setting(QStringLiteral("stadium_name")));
    updateLogoPreview();

    QStringList tactics = m_context.definitions().tacticNames();
    std::sort(tactics.begin(), tactics.end());
    const auto fillTacticCombo = [&tactics](QComboBox *combo, const QString &current) {
        combo->clear();
        combo->addItem(QString());
        combo->addItems(tactics);
        if (!current.isEmpty())
            combo->setCurrentText(current);
    };
    fillTacticCombo(m_favTactic1Combo,
                    m_context.database().setting(QStringLiteral("favorite_tactic_1")));
    fillTacticCombo(m_favTactic2Combo,
                    m_context.database().setting(QStringLiteral("favorite_tactic_2")));

    m_natNameEdit->setText(m_context.nationalTeamName());
    m_natCodeEdit->setText(m_context.nationalTeamCode());
    const int natAge = m_context.nationalTeamAgeLimit();
    m_natAgeSpin->setValue(natAge > 0 ? natAge : 99);
    fillTacticCombo(m_natFav1Combo,
                    m_context.database().setting(QStringLiteral("national_fav_tactic_1")));
    fillTacticCombo(m_natFav2Combo,
                    m_context.database().setting(QStringLiteral("national_fav_tactic_2")));

    for (auto it = m_weightSpins.begin(); it != m_weightSpins.end(); ++it)
        it.value()->setValue(config.weight(it.key()));
    for (auto it = m_gkWeightSpins.begin(); it != m_gkWeightSpins.end(); ++it)
        it.value()->setValue(config.gkWeight(it.key()));
    m_keyMultSpin->setValue(config.roleMultiplier(QStringLiteral("key")));
    m_prefMultSpin->setValue(config.roleMultiplier(QStringLiteral("preferable")));
    for (auto it = m_aptSpins.begin(); it != m_aptSpins.end(); ++it)
        it.value()->setValue(config.aptWeight(it.key()));

    m_outfielderAgeSpin->setValue(config.ageThreshold(QStringLiteral("outfielder")));
    m_goalkeeperAgeSpin->setValue(config.ageThreshold(QStringLiteral("goalkeeper")));
    m_naturalPosSpin->setValue(config.selectionBonus(QStringLiteral("natural_position")));
    m_maxDepthRolesSpin->setValue(
        config.squadManagementSetting(QStringLiteral("max_roles_per_depth_player")));
    m_minLoanTalentSpin->setValue(
        config.squadManagementSetting(QStringLiteral("min_loan_talent_score")));
    m_minLoanAgeSpin->setValue(config.squadManagementSetting(QStringLiteral("min_loan_age")));
    m_youthLoanOverAgeSpin->setValue(
        config.squadManagementSetting(QStringLiteral("youth_loan_over_age")));
    m_displacementSpin->setValue(
        config.gapAnalysisSetting(QStringLiteral("displacement_threshold")));
    m_dropoffSpin->setValue(config.gapAnalysisSetting(QStringLiteral("dropoff_threshold")));
    m_wrongSideSpin->setValue(config.gapAnalysisSetting(QStringLiteral("wrong_side_penalty")));

    const auto themeSettings = config.themeSettings();
    m_modeCombo->setCurrentIndex(
        themeSettings.value(QStringLiteral("current_mode")) == QLatin1String("day") ? 1 : 0);
    m_pendingPreset = themeSettings.value(QStringLiteral("theme_preset"),
                                          QStringLiteral("custom"));
    {
        QSignalBlocker blocker(m_presetCombo);
        const int presetIndex = m_presetCombo->findData(m_pendingPreset);
        m_presetCombo->setCurrentIndex(
            presetIndex >= 0 ? presetIndex
                             : m_presetCombo->findData(QStringLiteral("custom")));
    }
    m_pendingColors.clear();
    for (auto it = m_colorButtons.begin(); it != m_colorButtons.end(); ++it) {
        const QString value = themeSettings.value(it.key());
        m_pendingColors.insert(it.key(), value);
        it.value()->setText(value);
        it.value()->setStyleSheet(colorButtonStyle(QColor(value)));
    }
    updateContrastWarning();

    m_dbCombo->clear();
    m_dbCombo->addItems(m_context.availableDatabases());
    m_dbCombo->setCurrentText(m_context.currentDbName());
}

void SettingsPage::updateContrastWarning()
{
    QStringList warnings;
    for (const QString &mode : {QStringLiteral("night"), QStringLiteral("day")}) {
        const QColor text(m_pendingColors.value(mode + QStringLiteral("_text_color")));
        const QColor bg(m_pendingColors.value(mode + QStringLiteral("_background_color")));
        if (text.isValid() && bg.isValid()) {
            const double ratio = contrastRatio(text, bg);
            if (ratio < 4.5) {
                warnings << tr("⚠ %1: Kontrast Text/Hintergrund nur %2:1 (WCAG empfiehlt ≥ 4,5:1)")
                                .arg(mode == QLatin1String("night") ? tr("Nacht") : tr("Tag"))
                                .arg(ratio, 0, 'f', 1);
            }
        }
    }
    m_contrastLabel->setText(warnings.isEmpty() ? tr("✓ Kontrastwerte in Ordnung")
                                                : warnings.join(QLatin1Char('\n')));
}

void SettingsPage::saveAll()
{
    AppConfig &config = m_context.config();

    // --- Club tab (per-database settings) ---
    Database &db = m_context.database();
    const auto saveSetting = [&db](const QString &key, const QString &value) {
        if (value.isEmpty())
            db.removeSetting(key);
        else
            db.setSetting(key, value);
    };
    saveSetting(QStringLiteral("fm_version"), m_fmVersionCombo->currentData().toString());
    saveSetting(QStringLiteral("user_club"), m_userClubCombo->currentText());
    saveSetting(QStringLiteral("second_team_club"), m_secondClubCombo->currentText());
    saveSetting(QStringLiteral("club_country_code"),
                m_clubCountryEdit->text().trimmed().toUpper());
    saveSetting(QStringLiteral("full_club_name"), m_fullClubNameEdit->text().trimmed());
    saveSetting(QStringLiteral("stadium_name"), m_stadiumEdit->text().trimmed());
    saveSetting(QStringLiteral("favorite_tactic_1"), m_favTactic1Combo->currentText());
    saveSetting(QStringLiteral("favorite_tactic_2"), m_favTactic2Combo->currentText());
    saveSetting(QStringLiteral("national_team_name"), m_natNameEdit->text().trimmed());
    saveSetting(QStringLiteral("national_team_country_code"),
                m_natCodeEdit->text().trimmed().toUpper());
    saveSetting(QStringLiteral("national_team_age_limit"),
                QString::number(m_natAgeSpin->value()));
    saveSetting(QStringLiteral("national_fav_tactic_1"), m_natFav1Combo->currentText());
    saveSetting(QStringLiteral("national_fav_tactic_2"), m_natFav2Combo->currentText());

    // Detect whether any DWRS-relevant weight actually changed.
    bool weightsChanged = false;
    const auto noteChange = [&](double oldValue, double newValue) {
        if (!qFuzzyCompare(oldValue + 1.0, newValue + 1.0))
            weightsChanged = true;
    };

    for (auto it = m_weightSpins.begin(); it != m_weightSpins.end(); ++it) {
        noteChange(config.weight(it.key()), it.value()->value());
        config.setWeight(it.key(), it.value()->value());
    }
    for (auto it = m_gkWeightSpins.begin(); it != m_gkWeightSpins.end(); ++it) {
        noteChange(config.gkWeight(it.key()), it.value()->value());
        config.setGkWeight(it.key(), it.value()->value());
    }
    noteChange(config.roleMultiplier(QStringLiteral("key")), m_keyMultSpin->value());
    config.setRoleMultiplier(QStringLiteral("key"), m_keyMultSpin->value());
    noteChange(config.roleMultiplier(QStringLiteral("preferable")), m_prefMultSpin->value());
    config.setRoleMultiplier(QStringLiteral("preferable"), m_prefMultSpin->value());
    for (auto it = m_aptSpins.begin(); it != m_aptSpins.end(); ++it)
        config.setAptWeight(it.key(), it.value()->value());

    config.setAgeThreshold(QStringLiteral("outfielder"), m_outfielderAgeSpin->value());
    config.setAgeThreshold(QStringLiteral("goalkeeper"), m_goalkeeperAgeSpin->value());
    config.setSelectionBonus(QStringLiteral("natural_position"), m_naturalPosSpin->value());
    config.setSquadManagementSetting(QStringLiteral("max_roles_per_depth_player"),
                                     m_maxDepthRolesSpin->value());
    config.setSquadManagementSetting(QStringLiteral("min_loan_talent_score"),
                                     m_minLoanTalentSpin->value());
    config.setSquadManagementSetting(QStringLiteral("min_loan_age"), m_minLoanAgeSpin->value());
    config.setSquadManagementSetting(QStringLiteral("youth_loan_over_age"),
                                     m_youthLoanOverAgeSpin->value());
    config.setGapAnalysisSetting(QStringLiteral("displacement_threshold"),
                                 m_displacementSpin->value());
    config.setGapAnalysisSetting(QStringLiteral("dropoff_threshold"), m_dropoffSpin->value());
    config.setGapAnalysisSetting(QStringLiteral("wrong_side_penalty"), m_wrongSideSpin->value());

    auto themeSettings = config.themeSettings();
    themeSettings.insert(QStringLiteral("current_mode"),
                         m_modeCombo->currentData().toString());
    themeSettings.insert(QStringLiteral("theme_preset"),
                         m_pendingPreset.isEmpty() ? QStringLiteral("custom")
                                                   : m_pendingPreset);
    for (auto it = m_pendingColors.constBegin(); it != m_pendingColors.constEnd(); ++it)
        themeSettings.insert(it.key(), it.value());
    config.saveThemeSettings(themeSettings);

    m_context.reloadEngines();
    m_theme.setMode(m_modeCombo->currentData().toString());
    m_theme.apply();
    m_context.notifySettingsChanged();

    if (weightsChanged) {
        emit recalcRequested();
    } else {
        QMessageBox::information(this, tr("Einstellungen"), tr("Einstellungen gespeichert."));
    }
}

} // namespace fm
