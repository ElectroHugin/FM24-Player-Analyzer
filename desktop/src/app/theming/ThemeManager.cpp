#include "ThemeManager.h"

#include "core/AppConfig.h"
#include "core/Utils.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>

#include <functional>

namespace fm {

namespace {

// QSS cannot draw combo arrows / check marks without image assets, so the
// theme renders small PNGs matching the current color into the temp dir.
QString renderAsset(const QString &name, const QColor &color,
                    const std::function<void(QPainter &)> &draw)
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/fmplayeranalyzer");
    QDir().mkpath(dir);
    const QString path = QStringLiteral("%1/%2_%3.png").arg(dir, name, color.name().mid(1));

    if (!QFile::exists(path)) {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(color, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        draw(painter);
        painter.end();
        pixmap.save(path, "PNG");
    }
    return QString(path).replace(QLatin1Char('\\'), QLatin1Char('/'));
}

QString chevronDown(const QColor &color)
{
    return renderAsset(QStringLiteral("chevron"), color, [](QPainter &p) {
        p.drawPolyline(QPolygonF{{9.0, 13.0}, {16.0, 20.0}, {23.0, 13.0}});
    });
}

QString checkMark(const QColor &color)
{
    return renderAsset(QStringLiteral("check"), color, [](QPainter &p) {
        p.drawPolyline(QPolygonF{{8.0, 16.5}, {13.5, 22.0}, {24.0, 10.0}});
    });
}

// Readable black/white text on a filled color.
QColor contrastText(const QColor &background)
{
    return contrastRatio(background, QColorConstants::White) >= 3.0
               ? QColorConstants::White
               : QColorConstants::Black;
}

QString rgba(const QColor &c, double alpha)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(alpha);
}

} // namespace

const QStringList &ThemeManager::roleNames()
{
    static const QStringList roles = {
        QStringLiteral("background"), QStringLiteral("surface"),
        QStringLiteral("sidebar"),    QStringLiteral("header"),
        QStringLiteral("primary"),    QStringLiteral("interactive"),
        QStringLiteral("text"),
    };
    return roles;
}

QString ThemeManager::settingsKey(const QString &mode, const QString &role)
{
    return mode + QLatin1Char('_') + role + QStringLiteral("_color");
}

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

QColor ThemeManager::roleColor(const QString &role) const
{
    // themeDefaults() supplies a value for every role/mode, so this normally
    // resolves without the built-in fallback.
    const auto settings = m_config.themeSettings();
    const QString value = settings.value(settingsKey(m_mode, role));
    if (!value.isEmpty() && QColor::isValidColorName(value))
        return QColor(value);

    const bool night = m_mode != QLatin1String("day");
    static const QHash<QString, QColor> nightFallback = {
        {QStringLiteral("background"), QColor(QStringLiteral("#262626"))},
        {QStringLiteral("surface"), QColor(QStringLiteral("#343434"))},
        {QStringLiteral("sidebar"), QColor(QStringLiteral("#1f1f1f"))},
        {QStringLiteral("header"), QColor(QStringLiteral("#2d2d2d"))},
        {QStringLiteral("primary"), QColor(QStringLiteral("#3d7dff"))},
        {QStringLiteral("interactive"), QColor(QStringLiteral("#3d7dff"))},
        {QStringLiteral("text"), QColor(QStringLiteral("#F5F5F5"))},
    };
    static const QHash<QString, QColor> dayFallback = {
        {QStringLiteral("background"), QColor(QStringLiteral("#F0F2F6"))},
        {QStringLiteral("surface"), QColor(QStringLiteral("#FFFFFF"))},
        {QStringLiteral("sidebar"), QColor(QStringLiteral("#E7E9ED"))},
        {QStringLiteral("header"), QColor(QStringLiteral("#FFFFFF"))},
        {QStringLiteral("primary"), QColor(QStringLiteral("#0069b3"))},
        {QStringLiteral("interactive"), QColor(QStringLiteral("#0069b3"))},
        {QStringLiteral("text"), QColor(QStringLiteral("#31333F"))},
    };
    return (night ? nightFallback : dayFallback).value(role, QColor(QStringLiteral("#808080")));
}

void ThemeManager::apply()
{
    m_background = roleColor(QStringLiteral("background"));
    m_surface = roleColor(QStringLiteral("surface"));
    m_sidebar = roleColor(QStringLiteral("sidebar"));
    m_header = roleColor(QStringLiteral("header"));
    m_primary = roleColor(QStringLiteral("primary"));
    m_interactive = roleColor(QStringLiteral("interactive"));
    m_text = roleColor(QStringLiteral("text"));

    qApp->setStyleSheet(buildStyleSheet());
    emit themeChanged();
}

QString ThemeManager::buildStyleSheet() const
{
    const bool night = m_mode == QLatin1String("night");

    const QString bg = m_background.name();
    const QString surface = m_surface.name();
    const QString sidebar = m_sidebar.name();
    const QString header = m_header.name();
    const QString primary = m_primary.name();
    const QString interactive = m_interactive.name();
    const QString fg = m_text.name();

    const QString primaryText = contrastText(m_primary).name();
    const QString interactiveText = contrastText(m_interactive).name();
    const QString headerText = contrastText(m_header).name();
    const QString primaryHover = m_primary.darker(115).name();
    const QString interactiveHover = m_interactive.darker(115).name();

    // Derived neutrals (work on any background via alpha).
    const QString muted = rgba(m_text, 0.60);
    const QString border = rgba(m_text, night ? 0.14 : 0.18);
    const QString hover = rgba(m_text, night ? 0.08 : 0.06);
    const QString rowAlt = rgba(m_text, night ? 0.04 : 0.05);
    const QString interactiveSubtle = rgba(m_interactive, night ? 0.28 : 0.18);
    const QString sidebarSelText = contrastText(m_primary).name();

    const QString chevron = chevronDown(m_text);
    const QString check = checkMark(QColor(interactiveText));
    const QString sidebarHover = rgba(m_text, night ? 0.07 : 0.06);

    QString qss = QStringLiteral(R"(
/* ===== Base ===== */
QMainWindow, QDialog, QWizard { background-color: %BG%; color: %FG%; }
QWidget { color: %FG%; font-size: 10pt; }
QLabel { background: transparent; }
QLabel#kpiCaption { color: %MUTED%; }
QLabel#sectionTitle { font-size: 12pt; font-weight: 600; }
QStackedWidget, QScrollArea, QScrollArea > QWidget > QWidget { background-color: %BG%; }
QSplitter::handle { background: transparent; }

/* ===== Sidebar ===== */
QFrame#sidebar { background-color: %SIDEBAR%; border-right: 1px solid %BORDER%; }
QFrame#sidebar QListWidget { background: transparent; border: none; outline: none; }
QFrame#sidebar QListWidget::item {
    padding: 9px 12px; margin: 1px 6px; border-radius: 8px; color: %FG%; }
QFrame#sidebar QListWidget::item:hover:!selected { background-color: %SIDEBARHOVER%; }
QFrame#sidebar QListWidget::item:selected {
    background-color: %PRIMARY%; color: %SIDEBARSELTEXT%; font-weight: 600; }
QFrame#sidebar QListWidget::item:disabled {
    color: %MUTED%; padding: 12px 12px 4px 10px; background: transparent; }

/* ===== Header bar ===== */
QFrame#headerBar { background-color: %HEADER%; border-bottom: 1px solid %BORDER%; }
QFrame#headerBar QLabel { color: %HEADERTEXT%; }

/* ===== Cards ===== */
QFrame#kpiTile, QFrame#suggestionCard {
    background-color: %SURFACE%; border: 1px solid %BORDER%; border-radius: 10px; }
QLabel#kpiValue { font-size: 20pt; font-weight: 600; color: %PRIMARY%; }

/* ===== Buttons ===== */
QPushButton {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 7px 16px; min-height: 20px; }
QPushButton:hover { background-color: %HOVER%; border-color: %INTERACTIVE%; }
QPushButton:pressed { background-color: %INTERACTIVESUBTLE%; }
QPushButton:disabled { color: %MUTED%; border-color: %BORDER%; background: transparent; }
QPushButton:default { background-color: %PRIMARY%; color: %PRIMARYTEXT%; border-color: %PRIMARY%; font-weight: 600; }
QPushButton:default:hover { background-color: %PRIMARYHOVER%; border-color: %PRIMARYHOVER%; }
QPushButton:checked { background-color: %INTERACTIVESUBTLE%; border-color: %INTERACTIVE%; }
QToolButton {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 6px 12px; }
QToolButton:hover { background-color: %HOVER%; border-color: %INTERACTIVE%; }
QToolButton::menu-indicator { image: url(%CHEVRON%); width: 12px; height: 12px;
    subcontrol-position: right center; subcontrol-origin: padding; right: 6px; }

/* ===== Inputs ===== */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 6px 10px; min-height: 18px;
    selection-background-color: %INTERACTIVE%; selection-color: %INTERACTIVETEXT%; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QTextEdit:focus, QPlainTextEdit:focus { border: 2px solid %INTERACTIVE%; padding: 5px 9px; }
QLineEdit:disabled, QComboBox:disabled { color: %MUTED%; }
QComboBox::drop-down { border: none; width: 28px; }
QComboBox::down-arrow { image: url(%CHEVRON%); width: 14px; height: 14px; }
QComboBox QAbstractItemView {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 4px;
    selection-background-color: %INTERACTIVESUBTLE%; selection-color: %FG%; }
QComboBox QAbstractItemView::item { padding: 6px 10px; border-radius: 6px; min-height: 22px; }
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background: transparent; border: none; width: 20px; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(%CHEVRON%); width: 10px; height: 10px; }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(%CHEVRON%); width: 10px; height: 10px; }

/* ===== Check boxes / radios ===== */
QCheckBox, QRadioButton { spacing: 8px; padding: 3px 0; }
QCheckBox::indicator, QRadioButton::indicator { width: 18px; height: 18px; }
QCheckBox::indicator {
    border: 2px solid %BORDER%; border-radius: 5px; background: %SURFACE%; }
QCheckBox::indicator:hover { border-color: %INTERACTIVE%; }
QCheckBox::indicator:checked {
    background-color: %INTERACTIVE%; border-color: %INTERACTIVE%; image: url(%CHECK%); }
QRadioButton::indicator { border: 2px solid %BORDER%; border-radius: 10px; background: %SURFACE%; }
QRadioButton::indicator:hover { border-color: %INTERACTIVE%; }
QRadioButton::indicator:checked { border: 6px solid %INTERACTIVE%; background: %INTERACTIVETEXT%; }

/* ===== Lists / tables / trees ===== */
QListWidget, QListView { background-color: %SURFACE%; border: 1px solid %BORDER%;
    border-radius: 8px; outline: none; }
QListWidget::item, QListView::item { padding: 6px 10px; border-radius: 6px; }
QListWidget::item:selected, QListView::item:selected {
    background-color: %INTERACTIVESUBTLE%; color: %FG%; }
QListWidget::item:hover:!selected, QListView::item:hover:!selected { background-color: %HOVER%; }
QTableView, QTreeView {
    background-color: %BG%; alternate-background-color: %ROWALT%; color: %FG%;
    border: 1px solid %BORDER%; border-radius: 8px;
    gridline-color: transparent;
    selection-background-color: %INTERACTIVESUBTLE%; selection-color: %FG%; }
QHeaderView { background: transparent; }
QHeaderView::section {
    background-color: %BG%; color: %MUTED%; font-weight: 600;
    border: none; border-bottom: 2px solid %BORDER%; padding: 7px 10px; }
QHeaderView::section:hover { color: %FG%; }
QTableCornerButton::section { background-color: %BG%; border: none; border-bottom: 2px solid %BORDER%; }

/* ===== Tabs ===== */
QTabWidget::pane { border: 1px solid %BORDER%; border-radius: 8px; background-color: %BG%; top: -1px; }
QTabBar::tab {
    background: transparent; color: %MUTED%; padding: 9px 18px; margin-right: 4px;
    border: none; border-bottom: 3px solid transparent; }
QTabBar::tab:hover { color: %FG%; }
QTabBar::tab:selected { color: %INTERACTIVE%; border-bottom: 3px solid %INTERACTIVE%; font-weight: 600; }

/* ===== Group boxes as cards ===== */
QGroupBox {
    border: 1px solid %BORDER%; border-radius: 10px; margin-top: 14px;
    padding: 10px 4px 4px 4px; background-color: %SURFACE%; }
QGroupBox::title {
    subcontrol-origin: margin; left: 12px; padding: 0 6px;
    color: %INTERACTIVE%; font-weight: 600; }
QGroupBox::indicator { width: 18px; height: 18px; border: 2px solid %BORDER%;
    border-radius: 5px; background: %BG%; }
QGroupBox::indicator:hover { border-color: %INTERACTIVE%; }
QGroupBox::indicator:checked { background-color: %INTERACTIVE%; border-color: %INTERACTIVE%; image: url(%CHECK%); }

/* ===== Sliders ===== */
QSlider { min-height: 24px; }
QSlider::groove:horizontal { height: 5px; border-radius: 2px; background: %BORDER%; }
QSlider::sub-page:horizontal { height: 5px; border-radius: 2px; background: %INTERACTIVE%; }
QSlider::handle:horizontal {
    width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;
    background: %INTERACTIVE%; border: 2px solid %INTERACTIVETEXT%; }
QSlider::handle:horizontal:hover { background: %INTERACTIVEHOVER%; }

/* ===== Scrollbars ===== */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: %BORDER%; border-radius: 4px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: %MUTED%; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: %BORDER%; border-radius: 4px; min-width: 32px; }
QScrollBar::handle:horizontal:hover { background: %MUTED%; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ===== Menus / tooltips / statusbar / progress ===== */
QMenu { background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 10px; padding: 6px; }
QMenu::item { padding: 8px 28px 8px 14px; border-radius: 6px; }
QMenu::item:selected { background-color: %INTERACTIVESUBTLE%; }
QMenu::item:disabled { color: %MUTED%; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 6px 10px; }
QMenu::icon { padding-left: 8px; }
QToolTip { background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    padding: 6px 10px; }
QStatusBar { background-color: %SURFACE%; color: %MUTED%; border-top: 1px solid %BORDER%; }
QProgressBar { background-color: %SURFACE%; border: 1px solid %BORDER%;
    border-radius: 8px; text-align: center; color: %FG%; min-height: 18px; }
QProgressBar::chunk { background-color: %INTERACTIVE%; border-radius: 7px; }
QProgressDialog { background-color: %BG%; }
)");

    qss.replace(QLatin1String("%BG%"), bg);
    qss.replace(QLatin1String("%SURFACE%"), surface);
    qss.replace(QLatin1String("%SIDEBAR%"), sidebar);
    qss.replace(QLatin1String("%SIDEBARHOVER%"), sidebarHover);
    qss.replace(QLatin1String("%SIDEBARSELTEXT%"), sidebarSelText);
    qss.replace(QLatin1String("%HEADER%"), header);
    qss.replace(QLatin1String("%HEADERTEXT%"), headerText);
    qss.replace(QLatin1String("%FG%"), fg);
    qss.replace(QLatin1String("%PRIMARY%"), primary);
    qss.replace(QLatin1String("%PRIMARYTEXT%"), primaryText);
    qss.replace(QLatin1String("%PRIMARYHOVER%"), primaryHover);
    qss.replace(QLatin1String("%INTERACTIVE%"), interactive);
    qss.replace(QLatin1String("%INTERACTIVETEXT%"), interactiveText);
    qss.replace(QLatin1String("%INTERACTIVEHOVER%"), interactiveHover);
    qss.replace(QLatin1String("%INTERACTIVESUBTLE%"), interactiveSubtle);
    qss.replace(QLatin1String("%HOVER%"), hover);
    qss.replace(QLatin1String("%BORDER%"), border);
    qss.replace(QLatin1String("%MUTED%"), muted);
    qss.replace(QLatin1String("%ROWALT%"), rowAlt);
    qss.replace(QLatin1String("%CHEVRON%"), chevron);
    qss.replace(QLatin1String("%CHECK%"), check);
    return qss;
}

} // namespace fm
