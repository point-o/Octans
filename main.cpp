#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>

// Application entry point.
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(QStringLiteral(":/images/Octans.ico")));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/SpaceGrotesk.ttf"));
    QFont launchFont = a.font();
    launchFont.setFamily(QStringLiteral("Space Grotesk"));
    if (launchFont.pointSizeF() < 12) launchFont.setPointSizeF(12);
    a.setFont(launchFont);
    MainWindow w;
    w.show();
    return a.exec();
}
