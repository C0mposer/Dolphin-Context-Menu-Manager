#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Dolphin Context Menu Manager"));
    QApplication::setApplicationDisplayName(QStringLiteral("Dolphin Context Menu Manager"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("Dolphin Context Menu Manager"));
    QApplication::setDesktopFileName(QStringLiteral("io.github.dolphincontextmenumanager"));
    QApplication::setWindowIcon(QIcon::fromTheme(
        QStringLiteral("io.github.dolphincontextmenumanager"),
        QIcon::fromTheme(QStringLiteral("configure"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Create and manage context-menu actions for KDE Dolphin."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption screenshotOption(
        QStringLiteral("screenshot"),
        QStringLiteral("Save a window screenshot and exit (useful for UI testing)."),
        QStringLiteral("path"));
    parser.addOption(screenshotOption);
    parser.process(application);

    MainWindow window;
    window.show();

    if (parser.isSet(screenshotOption)) {
        const QString path = QFileInfo(parser.value(screenshotOption)).absoluteFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        window.resize(1120, 1100);
        QTimer::singleShot(400, &application, [&application, &window, path] {
            const bool saved = window.grab().save(path);
            application.exit(saved ? 0 : 2);
        });
    }

    return application.exec();
}
