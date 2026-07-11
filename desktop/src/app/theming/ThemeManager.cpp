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
// theme renders small PNGs matching the current text color into the temp dir.
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
    // QSS url() wants forward slashes.
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

} // namespace

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
    const bool night = m_mode == QLatin1String("night");

    // --- Design tokens, derived from the four user-configurable colors. ---
    const QString bg = m_background.name();
    const QString surface = m_secondaryBackground.name();
    const QString fg = m_text.name();
    const QString accent = m_primary.name();
    const QString accentText =
        contrastRatio(m_primary, QColorConstants::White) >= 3.0 ? QStringLiteral("#ffffff")
                                                                : QStringLiteral("#000000");
    const QString accentHover = m_primary.lighter(night ? 118 : 92).name();
    const QString hover = m_secondaryBackground.lighter(night ? 128 : 97).name();
    const QString border = m_secondaryBackground.lighter(night ? 160 : 88).name();
    const auto rgba = [](const QColor &c, double alpha) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue())
            .arg(alpha);
    };
    const QString muted = rgba(m_text, 0.62);
    const QString accentSubtle = rgba(m_primary, night ? 0.30 : 0.16);
    const QString rowAlt = rgba(m_text, night ? 0.035 : 0.045);

    const QString chevron = chevronDown(m_text);
    const QString check = checkMark(QColor(accentText));

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
QFrame#sidebar { background-color: %SURFACE%; border-right: 1px solid %BORDER%; }
QFrame#sidebar QListWidget { background: transparent; border: none; outline: none; }
QFrame#sidebar QListWidget::item {
    padding: 9px 12px; margin: 1px 6px; border-radius: 8px; color: %FG%; }
QFrame#sidebar QListWidget::item:hover:!selected { background-color: %HOVER%; }
QFrame#sidebar QListWidget::item:selected {
    background-color: %ACCENTSUBTLE%; color: %FG%; }
QFrame#sidebar QListWidget::item:disabled {
    color: %MUTED%; padding: 12px 12px 4px 10px; background: transparent; }

/* ===== Header bar ===== */
QFrame#headerBar { background-color: %SURFACE%; border-bottom: 1px solid %BORDER%; }

/* ===== Cards ===== */
QFrame#kpiTile, QFrame#suggestionCard {
    background-color: %SURFACE%; border: 1px solid %BORDER%; border-radius: 10px; }
QLabel#kpiValue { font-size: 20pt; font-weight: 600; color: %ACCENT%; }

/* ===== Buttons ===== */
QPushButton {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 7px 16px; min-height: 20px; }
QPushButton:hover { background-color: %HOVER%; border-color: %ACCENT%; }
QPushButton:pressed { background-color: %ACCENTSUBTLE%; }
QPushButton:disabled { color: %MUTED%; border-color: %BORDER%; background: transparent; }
QPushButton:default { background-color: %ACCENT%; color: %ACCENTTEXT%; border-color: %ACCENT%; font-weight: 600; }
QPushButton:default:hover { background-color: %ACCENTHOVER%; }
QPushButton:checked { background-color: %ACCENTSUBTLE%; border-color: %ACCENT%; }
QToolButton {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 6px 12px; }
QToolButton:hover { background-color: %HOVER%; border-color: %ACCENT%; }
QToolButton::menu-indicator { image: url(%CHEVRON%); width: 12px; height: 12px;
    subcontrol-position: right center; subcontrol-origin: padding; right: 6px; }

/* ===== Inputs ===== */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 6px 10px; min-height: 18px;
    selection-background-color: %ACCENT%; selection-color: %ACCENTTEXT%; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QTextEdit:focus, QPlainTextEdit:focus { border: 2px solid %ACCENT%; padding: 5px 9px; }
QLineEdit:disabled, QComboBox:disabled { color: %MUTED%; }
QComboBox::drop-down { border: none; width: 28px; }
QComboBox::down-arrow { image: url(%CHEVRON%); width: 14px; height: 14px; }
QComboBox QAbstractItemView {
    background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    border-radius: 8px; padding: 4px;
    selection-background-color: %ACCENTSUBTLE%; selection-color: %FG%; }
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
QCheckBox::indicator:hover { border-color: %ACCENT%; }
QCheckBox::indicator:checked {
    background-color: %ACCENT%; border-color: %ACCENT%; image: url(%CHECK%); }
QRadioButton::indicator { border: 2px solid %BORDER%; border-radius: 10px; background: %SURFACE%; }
QRadioButton::indicator:hover { border-color: %ACCENT%; }
QRadioButton::indicator:checked { border: 6px solid %ACCENT%; background: %ACCENTTEXT%; }

/* ===== Lists / tables / trees ===== */
QListWidget, QListView { background-color: %SURFACE%; border: 1px solid %BORDER%;
    border-radius: 8px; outline: none; }
QListWidget::item, QListView::item { padding: 6px 10px; border-radius: 6px; }
QListWidget::item:selected, QListView::item:selected {
    background-color: %ACCENTSUBTLE%; color: %FG%; }
QListWidget::item:hover:!selected, QListView::item:hover:!selected { background-color: %HOVER%; }
/* Cell painting (background/text/padding) is done by CellStyleDelegate so the
   model's DWRS/attribute colors survive; do NOT add a QTableView::item rule —
   it makes the stylesheet own item painting and drops those colors. */
QTableView, QTreeView {
    background-color: %BG%; alternate-background-color: %ROWALT%; color: %FG%;
    border: 1px solid %BORDER%; border-radius: 8px;
    gridline-color: transparent;
    selection-background-color: %ACCENTSUBTLE%; selection-color: %FG%; }
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
QTabBar::tab:selected { color: %ACCENT%; border-bottom: 3px solid %ACCENT%; font-weight: 600; }

/* ===== Group boxes as cards ===== */
QGroupBox {
    border: 1px solid %BORDER%; border-radius: 10px; margin-top: 14px;
    padding: 10px 4px 4px 4px; }
QGroupBox::title {
    subcontrol-origin: margin; left: 12px; padding: 0 6px;
    color: %ACCENT%; font-weight: 600; }
QGroupBox::indicator { width: 18px; height: 18px; border: 2px solid %BORDER%;
    border-radius: 5px; background: %SURFACE%; }
QGroupBox::indicator:hover { border-color: %ACCENT%; }
QGroupBox::indicator:checked { background-color: %ACCENT%; border-color: %ACCENT%; image: url(%CHECK%); }

/* ===== Sliders ===== */
QSlider { min-height: 24px; }
QSlider::groove:horizontal { height: 5px; border-radius: 2px; background: %BORDER%; }
QSlider::sub-page:horizontal { height: 5px; border-radius: 2px; background: %ACCENT%; }
QSlider::handle:horizontal {
    width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;
    background: %ACCENT%; border: 2px solid %ACCENTTEXT%; }
QSlider::handle:horizontal:hover { background: %ACCENTHOVER%; }

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
QMenu::item:selected { background-color: %ACCENTSUBTLE%; }
QMenu::item:disabled { color: %MUTED%; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 6px 10px; }
QMenu::icon { padding-left: 8px; }
QToolTip { background-color: %SURFACE%; color: %FG%; border: 1px solid %BORDER%;
    padding: 6px 10px; }
QStatusBar { background-color: %SURFACE%; color: %MUTED%; border-top: 1px solid %BORDER%; }
QProgressBar { background-color: %SURFACE%; border: 1px solid %BORDER%;
    border-radius: 8px; text-align: center; color: %FG%; min-height: 18px; }
QProgressBar::chunk { background-color: %ACCENT%; border-radius: 7px; }
QProgressDialog { background-color: %BG%; }
)");

    qss.replace(QLatin1String("%BG%"), bg);
    qss.replace(QLatin1String("%SURFACE%"), surface);
    qss.replace(QLatin1String("%FG%"), fg);
    qss.replace(QLatin1String("%ACCENT%"), accent);
    qss.replace(QLatin1String("%ACCENTTEXT%"), accentText);
    qss.replace(QLatin1String("%ACCENTHOVER%"), accentHover);
    qss.replace(QLatin1String("%ACCENTSUBTLE%"), accentSubtle);
    qss.replace(QLatin1String("%HOVER%"), hover);
    qss.replace(QLatin1String("%BORDER%"), border);
    qss.replace(QLatin1String("%MUTED%"), muted);
    qss.replace(QLatin1String("%ROWALT%"), rowAlt);
    qss.replace(QLatin1String("%CHEVRON%"), chevron);
    qss.replace(QLatin1String("%CHECK%"), check);
    return qss;
}

} // namespace fm
