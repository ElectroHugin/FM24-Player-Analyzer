#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>

#include "core/Version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(fm::appName());
    QApplication::setApplicationVersion(fm::appVersion());
    QApplication::setOrganizationName(QStringLiteral("FM24PlayerAnalyzer"));

    QMainWindow window;
    window.setWindowTitle(fm::appName());
    window.setCentralWidget(new QLabel(QStringLiteral("FM24 Player Analyzer — Skelett"), &window));
    window.resize(1600, 950);
    window.show();

    // --smoke: exit immediately after the event loop starts (build verification)
    if (app.arguments().contains(QStringLiteral("--smoke"))) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
