#include "MainWindow.h"

#include "AppContext.h"
#include "pages/AssignRolesPage.h"
#include "pages/DashboardPage.h"
#include "pages/PlaceholderPage.h"
#include "pages/RoleAnalysisPage.h"
#include "pages/SettingsPage.h"
#include "pages/SquadMatrixPage.h"
#include "pages/TransfersPage.h"
#include "theming/ThemeManager.h"
#include "core/Database.h"
#include "core/Version.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCompleter>
#include <QStandardItemModel>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace fm {

namespace {

struct MenuEntry {
    QString label;
    QString pageId;
};

// Club-mode menu (mirrors legacy sidebar order).
const QList<MenuEntry> &clubMenu()
{
    static const QList<MenuEntry> entries = {
        {QObject::tr("Dashboard"), QStringLiteral("dashboard")},
        {QObject::tr("Rollen zuweisen"), QStringLiteral("assign_roles")},
        {QObject::tr("Rollen-Analyse"), QStringLiteral("role_analysis")},
        {QObject::tr("Spieler-Profil"), QStringLiteral("player_profile")},
        {QObject::tr("Squad Matrix"), QStringLiteral("squad_matrix")},
        {QObject::tr("Best XI"), QStringLiteral("best_xi")},
        {QObject::tr("Gap-Analyse"), QStringLiteral("gap_analysis")},
        {QObject::tr("Taktik-Explorer"), QStringLiteral("tactic_explorer")},
        {QObject::tr("Transfers"), QStringLiteral("transfers")},
        {QObject::tr("Spieler-Vergleich"), QStringLiteral("player_comparison")},
        {QObject::tr("Entwicklung"), QStringLiteral("dwrs_development")},
        {QObject::tr("Spieler bearbeiten"), QStringLiteral("edit_player")},
    };
    return entries;
}

const QList<MenuEntry> &nationalMenu()
{
    static const QList<MenuEntry> entries = {
        {QObject::tr("National-Dashboard"), QStringLiteral("national_dashboard")},
        {QObject::tr("Rollen zuweisen"), QStringLiteral("assign_roles")},
        {QObject::tr("Kader-Auswahl"), QStringLiteral("national_squad_selection")},
        {QObject::tr("Squad Matrix"), QStringLiteral("national_squad_matrix")},
        {QObject::tr("Best XI"), QStringLiteral("national_best_xi")},
        {QObject::tr("Taktik-Explorer"), QStringLiteral("tactic_explorer")},
        {QObject::tr("Spieler-Profil"), QStringLiteral("player_profile")},
        {QObject::tr("Spieler-Vergleich"), QStringLiteral("player_comparison")},
        {QObject::tr("Entwicklung"), QStringLiteral("dwrs_development")},
    };
    return entries;
}

const QList<MenuEntry> &globalMenu()
{
    static const QList<MenuEntry> entries = {
        {QObject::tr("Neue Rolle"), QStringLiteral("new_role")},
        {QObject::tr("Neue Taktik"), QStringLiteral("new_tactic")},
        {QObject::tr("Einstellungen"), QStringLiteral("settings")},
    };
    return entries;
}

// Which milestone delivers each placeholder page (shown on the placeholder).
QString milestoneFor(const QString &pageId)
{
    static const QHash<QString, QString> milestones = {
        {QStringLiteral("player_profile"), QStringLiteral("M9")},
        {QStringLiteral("player_comparison"), QStringLiteral("M9")},
        {QStringLiteral("dwrs_development"), QStringLiteral("M9")},
        {QStringLiteral("edit_player"), QStringLiteral("M9")},
        {QStringLiteral("best_xi"), QStringLiteral("M10")},
        {QStringLiteral("gap_analysis"), QStringLiteral("M10")},
        {QStringLiteral("tactic_explorer"), QStringLiteral("M10")},
        {QStringLiteral("national_dashboard"), QStringLiteral("M10")},
        {QStringLiteral("national_squad_selection"), QStringLiteral("M10")},
        {QStringLiteral("national_squad_matrix"), QStringLiteral("M10")},
        {QStringLiteral("national_best_xi"), QStringLiteral("M10")},
        {QStringLiteral("new_role"), QStringLiteral("M11")},
        {QStringLiteral("new_tactic"), QStringLiteral("M11")},
    };
    return milestones.value(pageId, QStringLiteral("M12"));
}

} // namespace

MainWindow::MainWindow(AppContext &context, ThemeManager &theme, QWidget *parent)
    : QMainWindow(parent)
    , m_context(context)
    , m_theme(theme)
{
    setWindowTitle(appName());
    resize(1600, 950);
    setMinimumSize(1100, 700);
    if (!m_context.paths().windowGeometry().isEmpty())
        restoreGeometry(m_context.paths().windowGeometry());

    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    buildSidebar();
    rootLayout->addWidget(findChild<QFrame *>(QStringLiteral("sidebar")));

    m_stack = new QStackedWidget(central);
    rootLayout->addWidget(m_stack, 1);
    setCentralWidget(central);

    statusBar()->showMessage(tr("Bereit"));
    updateDbLabel();

    connect(&m_context, &AppContext::databaseChanged, this, [this](const QString &) {
        updateDbLabel();
        m_searchModelDirty = true;
        for (PageBase *page : std::as_const(m_pages))
            page->refresh();
    });
    connect(&m_context, &AppContext::dataChanged, this, [this] {
        m_searchModelDirty = true;
        updateDbLabel();
        if (auto *page = qobject_cast<PageBase *>(m_stack->currentWidget()))
            page->refresh();
    });
    connect(&m_recalcWatcher, &QFutureWatcher<RatingsUpdater::Result>::finished, this, [this] {
        const RatingsUpdater::Result result = m_recalcWatcher.result();
        if (m_recalcDialog) {
            m_recalcDialog->close();
            m_recalcDialog->deleteLater();
            m_recalcDialog = nullptr;
        }
        if (!result.success) {
            QMessageBox::critical(this, tr("Neuberechnung"), result.error);
            return;
        }
        statusBar()->showMessage(tr("DWRS neu berechnet: %1 Bewertungen, %2 geänderte "
                                    "Einträge gespeichert.")
                                     .arg(result.computed)
                                     .arg(result.inserted),
                                 10000);
        m_context.reloadFromDatabase();
    });

    navigateTo(QStringLiteral("dashboard"));
}

void MainWindow::buildSidebar()
{
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(240);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(10, 12, 10, 12);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("<b>FM24 Player Analyzer</b>"), sidebar);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    m_dbLabel = new QLabel(sidebar);
    m_dbLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_dbLabel);

    m_modeCombo = new QComboBox(sidebar);
    m_modeCombo->addItem(tr("🏟 Vereins-Management"), QStringLiteral("club"));
    m_modeCombo->addItem(tr("🌍 National-Management"), QStringLiteral("national"));
    layout->addWidget(m_modeCombo);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::rebuildMenu);

    m_searchEdit = new QLineEdit(sidebar);
    m_searchEdit->setPlaceholderText(tr("🔎 Spieler suchen…"));
    m_searchEdit->setClearButtonEnabled(true);
    layout->addWidget(m_searchEdit);

    m_searchModel = new QStandardItemModel(this);
    m_searchCompleter = new QCompleter(m_searchModel, this);
    m_searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_searchCompleter->setFilterMode(Qt::MatchContains);
    m_searchCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_searchCompleter->setMaxVisibleItems(12);
    m_searchEdit->setCompleter(m_searchCompleter);

    // The model over 80k players is built lazily on first keystroke and
    // invalidated whenever the store reloads.
    connect(m_searchEdit, &QLineEdit::textEdited, this, [this] {
        if (m_searchModelDirty)
            rebuildSearchModel();
    });
    connect(m_searchCompleter, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, [this](const QModelIndex &index) {
                const QString uid = index.data(Qt::UserRole).toString();
                if (uid.isEmpty())
                    return;
                m_context.setPendingProfileUid(uid);
                m_searchEdit->clear();
                navigateTo(QStringLiteral("player_profile"));
            });

    m_menu = new QListWidget(sidebar);
    layout->addWidget(m_menu, 1);
    connect(m_menu, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current, QListWidgetItem *) {
                // m_stack does not exist yet while the constructor builds the
                // sidebar (rebuildMenu selects the first row).
                if (!current || !m_stack)
                    return;
                const QString pageId = current->data(Qt::UserRole).toString();
                if (!pageId.isEmpty())
                    navigateTo(pageId);
            });

    auto *themeButton = new QPushButton(tr("🌓 Tag/Nacht wechseln"), sidebar);
    layout->addWidget(themeButton);
    connect(themeButton, &QPushButton::clicked, this, [this] { m_theme.toggle(); });

    rebuildMenu();
}

void MainWindow::rebuildMenu()
{
    const bool national = m_modeCombo->currentData().toString() == QLatin1String("national");

    m_menu->blockSignals(true);
    m_menu->clear();

    const auto addSection = [this](const QString &heading) {
        auto *item = new QListWidgetItem(heading, m_menu);
        item->setFlags(Qt::NoItemFlags);
        QFont font = item->font();
        font.setBold(true);
        font.setPointSizeF(font.pointSizeF() * 0.85);
        item->setFont(font);
    };
    const auto addEntries = [this](const QList<MenuEntry> &entries) {
        for (const MenuEntry &entry : entries) {
            auto *item = new QListWidgetItem(entry.label, m_menu);
            item->setData(Qt::UserRole, entry.pageId);
        }
    };

    addSection(national ? tr("NATIONALTEAM") : tr("VEREIN"));
    addEntries(national ? nationalMenu() : clubMenu());
    addSection(tr("ALLGEMEIN"));
    addEntries(globalMenu());

    m_menu->blockSignals(false);
    m_menu->setCurrentRow(1); // first real entry below the section header
}

PageBase *MainWindow::createPage(const QString &pageId)
{
    if (pageId == QLatin1String("dashboard"))
        return new DashboardPage(m_context, m_theme, this);
    if (pageId == QLatin1String("assign_roles"))
        return new AssignRolesPage(m_context, this);
    if (pageId == QLatin1String("role_analysis"))
        return new RoleAnalysisPage(m_context, this);
    if (pageId == QLatin1String("squad_matrix"))
        return new SquadMatrixPage(m_context, this);
    if (pageId == QLatin1String("transfers"))
        return new TransfersPage(m_context, this);
    if (pageId == QLatin1String("settings")) {
        auto *page = new SettingsPage(m_context, m_theme, this);
        connect(page, &SettingsPage::recalcRequested, this, &MainWindow::startDwrsRecalc);
        return page;
    }

    // Everything else arrives in M8-M11.
    QString label = pageId;
    for (const auto &entry : clubMenu() + nationalMenu() + globalMenu()) {
        if (entry.pageId == pageId) {
            label = entry.label;
            break;
        }
    }
    return new PlaceholderPage(m_context, label, milestoneFor(pageId), this);
}

void MainWindow::navigateTo(const QString &pageId)
{
    PageBase *page = m_pages.value(pageId);
    if (!page) {
        page = createPage(pageId);
        m_pages.insert(pageId, page);
        m_stack->addWidget(page);
    }
    m_stack->setCurrentWidget(page);
    page->refresh();
}

void MainWindow::startDwrsRecalc()
{
    if (m_recalcWatcher.isRunning())
        return;

    m_recalcDialog = new QProgressDialog(tr("DWRS-Bewertungen werden neu berechnet…"),
                                         QString(), 0, 100, this);
    m_recalcDialog->setWindowModality(Qt::WindowModal);
    m_recalcDialog->setMinimumDuration(0);
    m_recalcDialog->setValue(0);

    // Snapshot everything the worker needs; it opens its own DB connection.
    const QString dbFile = m_context.database().filePath();
    std::vector<Player> players = m_context.store().players();
    const QStringList validRoles = m_context.definitions().validRoles();
    // The engine reads Definitions/AppConfig only during reloadConfig(), which
    // ran on the UI thread; calculateRole afterwards touches cached plans only.
    const DwrsEngine *engine = &m_context.dwrsEngine();

    auto *dialog = m_recalcDialog;
    m_recalcWatcher.setFuture(QtConcurrent::run(
        [dbFile, players = std::move(players), engine, validRoles, dialog] {
            Database db(QStringLiteral("recalc_worker"));
            if (!db.open(dbFile)) {
                RatingsUpdater::Result result;
                result.error = db.errorString();
                return result;
            }
            return RatingsUpdater::updateDwrsRatings(
                db, players, *engine, validRoles, {}, [dialog](int current, int total) {
                    const int percent = total > 0 ? current * 100 / total : 0;
                    QMetaObject::invokeMethod(
                        dialog, [dialog, percent] { dialog->setValue(percent); },
                        Qt::QueuedConnection);
                });
        }));
}

void MainWindow::rebuildSearchModel()
{
    m_searchModel->clear();
    const auto &players = m_context.store().players();
    m_searchModel->setRowCount(static_cast<int>(players.size()));
    int row = 0;
    for (const Player &player : players) {
        auto *item = new QStandardItem(QStringLiteral("%1 — %2 · %3")
                                           .arg(player.name, player.club, player.positionRaw));
        item->setData(player.uid, Qt::UserRole);
        m_searchModel->setItem(row++, 0, item);
    }
    m_searchModelDirty = false;
}

void MainWindow::updateDbLabel()
{
    m_dbLabel->setText(tr("Datenbank: <b>%1</b> · %2 Spieler")
                           .arg(m_context.currentDbName())
                           .arg(m_context.store().size()));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_context.paths().setWindowGeometry(saveGeometry());
    QMainWindow::closeEvent(event);
}

} // namespace fm
