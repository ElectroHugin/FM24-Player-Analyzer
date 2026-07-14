#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QMainWindow>

#include "core/RatingsUpdater.h"

class QComboBox;
class QCompleter;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QProgressDialog;
class QStackedWidget;

namespace fm {

class AppContext;
class PageBase;
class PlayerSearchModel;
class ThemeManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(AppContext &context, ThemeManager &theme, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildSidebar();
    void buildMenuBar();
    void changeLanguage(const QString &language);
    void rebuildMenu();
    void navigateTo(const QString &pageId);
    PageBase *createPage(const QString &pageId);
    void startDwrsRecalc();
    void updateDbLabel();
    void updateHeader();
    void rebuildSearchModel();

    AppContext &m_context;
    ThemeManager &m_theme;

    QListWidget *m_menu = nullptr;
    QComboBox *m_modeCombo = nullptr; // Club / National
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_dbLabel = nullptr;
    QLabel *m_headerLogo = nullptr;     // optional club logo (user PNG)
    QLabel *m_headerIdentity = nullptr; // club/national name + stadium
    QLabel *m_headerSave = nullptr;     // active save file
    QStackedWidget *m_stack = nullptr;
    QHash<QString, PageBase *> m_pages; // lazily created

    QFutureWatcher<RatingsUpdater::Result> m_recalcWatcher;
    QProgressDialog *m_recalcDialog = nullptr;

    QCompleter *m_searchCompleter = nullptr;
    PlayerSearchModel *m_searchModel = nullptr;
    bool m_searchModelDirty = true;
};

} // namespace fm
