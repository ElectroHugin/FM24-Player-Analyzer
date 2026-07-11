#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QTimer>

#include "AppContext.h"
#include "FirstRunDialog.h"
#include "MainWindow.h"
#include "theming/ThemeManager.h"
#include "core/Version.h"

namespace {

// Programmatic app icon: a mini pitch with center circle — no asset files.
QIcon makeAppIcon()
{
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal s = size;
        painter.setBrush(QColor(0x2a, 0x5d, 0x34));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(0, 0, s, s), s * 0.2, s * 0.2);
        QPen line(QColor(255, 255, 255, 220), qMax<qreal>(1.0, s * 0.05));
        painter.setPen(line);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(s * 0.08, s * 0.5), QPointF(s * 0.92, s * 0.5));
        painter.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.18, s * 0.18);
        painter.drawRect(QRectF(s * 0.3, 0, s * 0.4, s * 0.16));
        painter.drawRect(QRectF(s * 0.3, s * 0.84, s * 0.4, s * 0.16));
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(fm::appName());
    QApplication::setApplicationVersion(fm::appVersion());
    QApplication::setOrganizationName(QStringLiteral("FM24PlayerAnalyzer"));
    QApplication::setWindowIcon(makeAppIcon());

    QFont appFont(QStringLiteral("Segoe UI"), 10);
    appFont.setHintingPreference(QFont::PreferFullHinting);
    app.setFont(appFont);

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
