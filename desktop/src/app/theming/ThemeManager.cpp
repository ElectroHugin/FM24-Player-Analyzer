#include "ThemeManager.h"

#include "core/AppConfig.h"
#include "core/Utils.h"

#include <QApplication>

namespace fm {

ThemeManager::ThemeManager(AppConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_mode = m_config.themeSettings().value(QStringLiteral("current_mode"),
                                            QStringLiteral("night"));
}

void ThemeManager::setMode(const QString &mode)
{
    if (mode == m_mode)
        return;
    m_mode = mode;
    auto settings = m_config.themeSettings();
    settings.insert(QStringLiteral("current_mode"), mode);
    m_config.saveThemeSettings(settings);
    apply();
}

void ThemeManager::toggle()
{
    setMode(m_mode == QLatin1String("night") ? QStringLiteral("day") : QStringLiteral("night"));
}

void ThemeManager::apply()
{
    const auto settings = m_config.themeSettings();
    const auto color = [&](const char *key, const char *fallback) {
        return QColor(settings.value(m_mode + QLatin1Char('_') + QLatin1String(key),
                                     QLatin1String(fallback)));
    };
    m_primary = color("primary_color", "#0055a4");
    m_text = color("text_color", m_mode == QLatin1String("night") ? "#FFFFFF" : "#31333F");
    m_background =
        color("background_color", m_mode == QLatin1String("night") ? "#0E1117" : "#F0F2F6");
    m_secondaryBackground = color("secondary_background_color",
                                  m_mode == QLatin1String("night") ? "#262730" : "#FFFFFF");

    qApp->setStyleSheet(buildStyleSheet());
    emit themeChanged();
}

QString ThemeManager::buildStyleSheet() const
{
    const QString bg = m_background.name();
    const QString bg2 = m_secondaryBackground.name();
    const QString fg = m_text.name();
    const QString accent = m_primary.name();
    const QString accentText =
        contrastRatio(m_primary, QColorConstants::White) >= 3.0 ? QStringLiteral("#ffffff")
                                                                : QStringLiteral("#000000");
    // Hover/pressed shades derived from the secondary background.
    const QString hover = m_secondaryBackground.lighter(m_mode == QLatin1String("night") ? 130 : 96).name();
    const QString border = m_secondaryBackground.lighter(m_mode == QLatin1String("night") ? 160 : 88).name();
    // Muted text for captions/footnotes.
    const QString muted = QStringLiteral("rgba(%1,%2,%3,0.65)")
                              .arg(m_text.red())
                              .arg(m_text.green())
                              .arg(m_text.blue());

    return QStringLiteral(R"(
QMainWindow, QDialog, QWizard { background-color: %1; color: %3; }
QWidget { color: %3; }
QLabel { background: transparent; }
QFrame#sidebar { background-color: %2; border-right: 1px solid %6; }
QListWidget { background-color: %2; color: %3; border: none; outline: none; }
QListWidget::item { padding: 7px 12px; border-radius: 4px; }
QListWidget::item:selected { background-color: %4; color: %5; }
QListWidget::item:hover:!selected { background-color: %7; }
QStackedWidget, QScrollArea, QScrollArea > QWidget > QWidget { background-color: %1; }
QTabWidget::pane { border: 1px solid %6; background-color: %1; }
QTabBar::tab { background: %2; color: %3; padding: 6px 14px; border: 1px solid %6; border-bottom: none; }
QTabBar::tab:selected { background: %4; color: %5; }
QPushButton { background-color: %2; color: %3; border: 1px solid %6; border-radius: 4px; padding: 6px 14px; }
QPushButton:hover { background-color: %7; }
QPushButton:default { background-color: %4; color: %5; border-color: %4; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit {
    background-color: %2; color: %3; border: 1px solid %6; border-radius: 4px; padding: 4px 6px; }
QComboBox QAbstractItemView { background-color: %2; color: %3; selection-background-color: %4; selection-color: %5; }
QTableView, QTreeView { background-color: %1; alternate-background-color: %2; color: %3;
    gridline-color: %6; selection-background-color: %4; selection-color: %5; }
QHeaderView::section { background-color: %2; color: %3; border: none; border-right: 1px solid %6;
    border-bottom: 1px solid %6; padding: 5px 8px; }
QGroupBox { border: 1px solid %6; border-radius: 4px; margin-top: 12px; padding-top: 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
QProgressBar { background-color: %2; border: 1px solid %6; border-radius: 4px; text-align: center; color: %3; }
QProgressBar::chunk { background-color: %4; }
QStatusBar { background-color: %2; color: %3; }
QMenu { background-color: %2; color: %3; border: 1px solid %6; }
QMenu::item:selected { background-color: %4; color: %5; }
QScrollBar:vertical { background: %1; width: 12px; }
QScrollBar::handle:vertical { background: %6; border-radius: 5px; min-height: 24px; }
QScrollBar:horizontal { background: %1; height: 12px; }
QScrollBar::handle:horizontal { background: %6; border-radius: 5px; min-width: 24px; }
QToolTip { background-color: %2; color: %3; border: 1px solid %6; }
QFrame#kpiTile { background-color: %2; border: 1px solid %6; border-radius: 8px; }
QLabel#kpiValue { font-size: 20pt; font-weight: 600; color: %4; }
QLabel#kpiCaption { color: %8; }
QFrame#suggestionCard { background-color: %2; border: 1px solid %6; border-radius: 8px; }
QLabel#sectionTitle { font-size: 12pt; font-weight: 600; }
)")
        .arg(bg, bg2, fg, accent, accentText, border, hover, muted);
}

} // namespace fm
