#pragma once

#include "PageBase.h"

#include <QHash>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace fm {

class ThemeManager;

// Central settings page (M6 scope: weights, thresholds, theme, database
// management, data dir display). Asset upload, prune and WCAG live-preview
// polish follow in M11.
class SettingsPage : public PageBase
{
    Q_OBJECT

public:
    SettingsPage(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

    void refresh() override;

signals:
    void recalcRequested(); // weights changed -> MainWindow runs the recalc

private:
    QWidget *buildClubTab();
    QWidget *buildWeightsTab();
    QWidget *buildThresholdsTab();
    QWidget *buildThemeTab();
    QWidget *buildDatabaseTab();
    void saveAll();
    void updateContrastWarning();

    ThemeManager &m_theme;

    // Weights tab widgets keyed by category / option name.
    QHash<QString, QDoubleSpinBox *> m_weightSpins;
    QHash<QString, QDoubleSpinBox *> m_gkWeightSpins;
    QDoubleSpinBox *m_keyMultSpin = nullptr;
    QDoubleSpinBox *m_prefMultSpin = nullptr;
    QHash<QString, QDoubleSpinBox *> m_aptSpins;

    // Thresholds tab.
    QSpinBox *m_outfielderAgeSpin = nullptr;
    QSpinBox *m_goalkeeperAgeSpin = nullptr;
    QDoubleSpinBox *m_naturalPosSpin = nullptr;
    QSpinBox *m_maxDepthRolesSpin = nullptr;
    QSpinBox *m_minLoanTalentSpin = nullptr;
    QDoubleSpinBox *m_displacementSpin = nullptr;
    QDoubleSpinBox *m_dropoffSpin = nullptr;
    QDoubleSpinBox *m_wrongSideSpin = nullptr;

    // Theme tab: color buttons per mode+key.
    QComboBox *m_modeCombo = nullptr;
    QHash<QString, QPushButton *> m_colorButtons; // "night_primary_color" etc.
    QHash<QString, QString> m_pendingColors;
    QLabel *m_contrastLabel = nullptr;

    // Database tab.
    QComboBox *m_dbCombo = nullptr;

    // Club tab (settings table keys, same as legacy).
    QComboBox *m_fmVersionCombo = nullptr;
    QComboBox *m_userClubCombo = nullptr;
    QComboBox *m_secondClubCombo = nullptr;
    QLineEdit *m_clubCountryEdit = nullptr;
    QLineEdit *m_fullClubNameEdit = nullptr;
    QLineEdit *m_stadiumEdit = nullptr;
    QComboBox *m_favTactic1Combo = nullptr;
    QComboBox *m_favTactic2Combo = nullptr;
    QLineEdit *m_natNameEdit = nullptr;
    QLineEdit *m_natCodeEdit = nullptr;
    QSpinBox *m_natAgeSpin = nullptr;
    QComboBox *m_natFav1Combo = nullptr;
    QComboBox *m_natFav2Combo = nullptr;
};

} // namespace fm
