// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("onthedial"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("otd.oh2gba.eu"));
    QCoreApplication::setApplicationName(QStringLiteral("otd"));
    QApplication::setApplicationDisplayName(QStringLiteral("On The Dial"));
    QCoreApplication::setApplicationVersion(QStringLiteral(OTD_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("otd"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Shortwave station identifier following your rig via Hamlib rigctld"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dataDirOpt(
        {QStringLiteral("d"), QStringLiteral("data-dir")},
        QStringLiteral("Keep settings and the station database in <dir> instead of the "
                       "user's application data directory (portable mode)."),
        QStringLiteral("dir"));
    parser.addOption(dataDirOpt);
    QCommandLineOption freqOpt(
        {QStringLiteral("f"), QStringLiteral("frequency")},
        QStringLiteral("Start in manual mode at <kHz> instead of following the rig."),
        QStringLiteral("kHz"));
    parser.addOption(freqOpt);
    QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                               QStringLiteral("Save a picture of the window to <file> after "
                                              "a few seconds and exit (for documentation)."),
                               QStringLiteral("file"));
    parser.addOption(shotOpt);
    parser.process(app);

    QString dataDir;
    if (parser.isSet(dataDirOpt))
    {
        dataDir = QDir(parser.value(dataDirOpt)).absolutePath();
        QDir().mkpath(dataDir);
    }
    else
    {
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    MainWindow w(dataDir, parser.value(freqOpt).toDouble());
    w.show();
    if (parser.isSet(shotOpt))
        w.screenshotTo(parser.value(shotOpt), 8000);
    return app.exec();
}
