#include "mainwindow.h"
#include "captureflow.h"
#include "desktopcapture.h"
#include <QApplication>
#include <QTest>
#include <QPushButton>
#include <QSignalSpy>
#include <QPointer>
#include <QFontDatabase>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
int main(int argc,char **argv) {
 QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf"); app.setFont(QFont("Arial",12));
 int count=0; auto check=[&count](bool ok){ ++count; if(!ok) qFatal("Capture smoke failed at check %d",count); };
 QWidget underlying(nullptr,Qt::FramelessWindowHint); underlying.setGeometry(0,0,640,480);
 underlying.setAutoFillBackground(true); QPalette backing=underlying.palette();
 backing.setColor(QPalette::Window,QColor(40,100,160)); underlying.setPalette(backing); underlying.show(); underlying.raise();
 MainWindow main; main.show(); app.processEvents();
 main.findChild<QPushButton *>("newCaptureButton")->click(); app.processEvents();
 auto *selector=main.findChild<CaptureSelector *>(); check(selector && selector->isVisible() && !main.isVisible());
 QTest::keyClick(selector,Qt::Key_Escape); app.sendPostedEvents(nullptr,QEvent::DeferredDelete); app.processEvents(); check(main.isVisible());
 main.findChild<QPushButton *>("newCaptureButton")->click(); app.processEvents();
 selector=main.findChild<CaptureSelector *>(); QSignalSpy selected(selector,&CaptureSelector::regionSelected);
 QTest::mousePress(selector,Qt::LeftButton,Qt::NoModifier,QPoint(300,260));
 QTest::mouseMove(selector,QPoint(100,100)); selector->grab().save("selection-test.png");
 QTest::mouseRelease(selector,Qt::LeftButton,Qt::NoModifier,QPoint(100,100)); app.processEvents();
 check(selected.size()==1);
 auto *preview=main.findChild<CapturePreview *>(); check(preview && preview->isVisible());
 const QRect region=selected.at(0).at(0).toRect();
 check(region.size()==QSize(201,161)); check(preview->geometry()==region);
 check(preview->windowFlags().testFlag(Qt::WindowTransparentForInput));
 check(preview->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
 check(preview->grab().toImage().pixelColor(100,100).alpha()==90);
 preview->grab().save("capture-test.png");
#ifdef Q_OS_WIN
 if (app.platformName()==QStringLiteral("windows")) {
     const HWND surface=reinterpret_cast<HWND>(preview->winId());
     const LONG_PTR style=GetWindowLongPtr(surface,GWL_EXSTYLE);
     check((style & WS_EX_TRANSPARENT) && (style & WS_EX_NOACTIVATE));
     POINT point{100,100}; ClientToScreen(surface,&point);
     check(WindowFromPoint(point)!=surface);
     auto *worker=preview->findChild<DesktopCapture *>(); check(worker!=nullptr);
     QSignalSpy frames(worker,&DesktopCapture::frameReady);
     QTest::qWait(400); check(!frames.isEmpty());
     check(qvariant_cast<QImage>(frames.last().at(0)).pixelColor(70,70)==QColor(40,100,160));
     backing.setColor(QPalette::Window,QColor(160,60,40)); underlying.setPalette(backing);
     QTest::qWait(400);
     check(qvariant_cast<QImage>(frames.last().at(0)).pixelColor(70,70)==QColor(160,60,40));
 }
#endif
 auto *handle=preview->findChild<QPushButton *>("captureHandle"); const QPoint old=preview->pos();
 QTest::keyClick(handle,Qt::Key_Right); check(preview->pos()==old+QPoint(10,0));
 auto *controls=preview->findChild<QWidget *>("captureControls");
 check(controls->pos()==preview->pos()); check(!controls->mask().contains(QPoint(100,20)));
 check(handle->geometry()==QRect(0,0,44,44));
 preview->close(); app.sendPostedEvents(nullptr,QEvent::DeferredDelete); app.processEvents(); check(main.isVisible());
 main.findChild<QPushButton *>("newCaptureButton")->click(); app.processEvents(); selector=main.findChild<CaptureSelector *>();
 QTest::mouseClick(selector,Qt::LeftButton,Qt::NoModifier,QPoint(30,30)); check(selector->isVisible());
 QTest::keyClick(selector,Qt::Key_Right); QTest::keyClick(selector,Qt::Key_Return); app.processEvents();
 preview=main.findChild<CapturePreview *>(); check(preview && preview->isVisible()); preview->close();
 app.sendPostedEvents(nullptr,QEvent::DeferredDelete); app.processEvents(); check(main.isVisible());
}
