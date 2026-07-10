#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include "AppContext.h"
#include "FirstRunDialog.h"
#include "MainWindow.h"
#include "theming/ThemeManager.h"
#include "core/Version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(fm::appName());
    QApplication::setApplicationVersion(fm::appVersion());
    QApplication::setOrganizationName(QStringLiteral("FM24PlayerAnalyzer"));

    fm::AppContext context;

    if (context.paths().isFirstRun()) {
        fm::FirstRunDialog dialog(fm::AppPaths::defaultDataDir());
        if (dialog.exec() != QDialog::Accepted)
            return 0;
        context.paths().setDataDir(dialog.chosenDataDir());
    }

    QString error;
    if (!context.initialize(&error)) {
        QMessageBox::critical(nullptr, fm::appName(),
                              QObject::tr("Start fehlgeschlagen:\n%1").arg(error));
        return 1;
    }

    fm::ThemeManager theme(context.config());
    theme.apply();

    fm::MainWindow window(context, theme);
    window.show();

    // --smoke: exit immediately after the event loop starts (build verification)
    if (app.arguments().contains(QStringLiteral("--smoke"))) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
