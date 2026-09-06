#include "mainwindow.h"

#include <QApplication>

// Application entry point.
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFont launchFont = a.font();
    launchFont.setFamily(QStringLiteral("Arial"));
    if (launchFont.pointSizeF() < 12) launchFont.setPointSizeF(12);
    a.setFont(launchFont);
    MainWindow w;
    w.show();
    return a.exec();
}
