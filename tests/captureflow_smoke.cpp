#include "mainwindow.h"
#include "captureflow.h"
#include "desktopcapture.h"
#include <QApplication>
#include <QTest>
#include <QPushButton>
#include <QSignalSpy>
#include <QPointer>
#include <QFontDatabase>
#include <QtMath>
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
 check(preview->grab().toImage().pixelColor(100,100).alpha()==0);
 const QImage outline=preview->grab().toImage();
 check(outline.pixelColor(0,0).alpha()==0);
 check(outline.pixelColor(qFloor(100*outline.devicePixelRatio()),qFloor(0.5*outline.devicePixelRatio()))==QColor(Qt::black));
 check(outline.pixelColor(qFloor(100*outline.devicePixelRatio()),qFloor(1.5*outline.devicePixelRatio()))==QColor(Qt::white));
 preview->grab().save("capture-test.png");
 check(preview->findChild<ModePopup *>("simulationMenu"));
 check(preview->findChild<ModeCircleButton *>("modeProtanopia"));
 check(preview->findChild<ModeCircleButton *>("modeDeuteranopia"));
 check(preview->findChild<ModeCircleButton *>("modeTritanopia"));
 auto *toggleHandle=preview->findChild<CaptureHandle *>("captureHandle");
 auto *togglePopup=preview->findChild<ModePopup *>("simulationMenu");
 toggleHandle->click(); app.processEvents(); check(togglePopup->isVisible());
 toggleHandle->click(); app.processEvents(); check(!togglePopup->isVisible());
 toggleHandle->click(); app.processEvents(); check(togglePopup->isVisible());
 toggleHandle->click(); app.processEvents(); check(!togglePopup->isVisible());
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
     check(qvariant_cast<QImage>(frames.last().at(0)).pixelColor(70,70)==QColor(40,100,160)); frames.clear();
     backing.setColor(QPalette::Window,QColor(160,60,40)); underlying.setPalette(backing);
     QTest::qWait(400);
     check(qvariant_cast<QImage>(frames.last().at(0)).pixelColor(70,70)==QColor(160,60,40)); frames.clear();
     auto *modeHandle=preview->findChild<CaptureHandle *>("captureHandle"); check(modeHandle);
     auto *popup=preview->findChild<ModePopup *>("simulationMenu"); check(popup);
     // Arrow keys navigate the popup; Return activates the focused circle (tritanopia).
     modeHandle->click(); app.processEvents(); check(popup->isVisible());
     QTest::keyClick(popup,Qt::Key_Down);
     QTest::keyClick(popup,Qt::Key_Down);
     QTest::keyClick(popup,Qt::Key_Down);
     QTest::keyClick(popup->focusWidget(),Qt::Key_Return); QTest::qWait(350);
     QImage tritanExpected(1,1,QImage::Format_RGB32); tritanExpected.fill(QColor(160,60,40));
     check(PixelTransform::applyInPlace(tritanExpected,PixelTransform::Mode::Tritanopia));
     check(qvariant_cast<QImage>(frames.last().at(0)).pixelColor(70,70)==tritanExpected.pixelColor(0,0)); frames.clear();
     check(modeHandle->mode()==PixelTransform::Mode::Tritanopia);
     const char *modeButtons[]={"modeProtanopia","modeDeuteranopia","modeTritanopia"};
     for (int mode=1;mode<=3;++mode) {
         QImage expected(1,1,QImage::Format_RGB32); expected.fill(QColor(160,60,40));
         check(PixelTransform::applyInPlace(expected,static_cast<PixelTransform::Mode>(mode)));
         auto *button=preview->findChild<ModeCircleButton *>(modeButtons[mode-1]); check(button);
         modeHandle->click(); app.processEvents(); check(popup->isVisible());
         check(button->mode()==static_cast<PixelTransform::Mode>(mode));
         button->click(); QTest::qWait(350);
         check(!popup->isVisible());
         check(qvariant_cast<QImage>(frames.last().at(0)).pixelColor(70,70)==expected.pixelColor(0,0)); frames.clear();
         check(modeHandle->mode()==static_cast<PixelTransform::Mode>(mode));
     }
// Clicking the active circle toggles back to Original.
      auto *active=preview->findChild<ModeCircleButton *>("modeTritanopia"); check(active);
      modeHandle->click(); app.processEvents();
      active->click(); QTest::qWait(250);
      check(qvariant_cast<QImage>(frames.last().at(0)).pixelColor(70,70)==QColor(160,60,40)); frames.clear();
      check(modeHandle->mode()==PixelTransform::Mode::Original);
  }
#endif
 auto *handle=preview->findChild<QPushButton *>("captureHandle"); const QPoint old=preview->pos();
 const QImage beforeMove=preview->grab().toImage();
 QTest::keyClick(handle,Qt::Key_Right); check(preview->pos()==old+QPoint(10,0));
 check(preview->grab().toImage().pixelColor(100,100)==beforeMove.pixelColor(100,100));
 auto *controls=preview->findChild<QWidget *>("captureControls");
 check(controls->pos()==preview->pos()); check(!controls->mask().contains(QPoint(100,20)));
 check(handle->geometry()==QRect(0,0,44,44));
 auto *resizeHandle=preview->findChild<QPushButton *>("captureResizeHandle"); check(resizeHandle);
 const QSize initialSize=preview->size();
 QTest::keyClick(resizeHandle,Qt::Key_Right); QTest::keyClick(resizeHandle,Qt::Key_Down);
 check(preview->size()==initialSize+QSize(10,10));
 check(controls->geometry()==preview->geometry());
 check(controls->mask().contains(QPoint(preview->width()-14,preview->height()-14)));
 check(!controls->mask().contains(preview->rect().center()));
 const QSize beforeDrag=preview->size();
 QTest::mousePress(resizeHandle,Qt::LeftButton,Qt::NoModifier,QPoint(14,14));
 QTest::mouseMove(resizeHandle,QPoint(34,29));
 check(preview->size()==beforeDrag+QSize(20,15));
 QTest::mouseRelease(resizeHandle,Qt::LeftButton,Qt::NoModifier,QPoint(14,14));
 preview->resize(1,1); check(preview->size()==QSize(88,72));
 QTest::keyClick(resizeHandle,Qt::Key_Left); QTest::keyClick(resizeHandle,Qt::Key_Up);
 check(preview->size()==QSize(88,72));
 preview->resize(initialSize);
 // A completed asynchronous frame must still paint if movement has already
 // requested a newer region; otherwise continuous dragging freezes the image.
 QImage completed(initialSize,QImage::Format_RGB32); completed.fill(Qt::red);
 const QRect previousArea=preview->geometry(); preview->move(preview->pos()+QPoint(10,0));
 check(QMetaObject::invokeMethod(preview,"presentFrame",Qt::DirectConnection,
       Q_ARG(QImage,completed),Q_ARG(QRect,previousArea)));
 check(preview->grab().toImage().pixelColor(50,50)==QColor(Qt::red));
 completed.fill(Qt::blue); preview->move(preview->pos()+QPoint(10,0));
 check(QMetaObject::invokeMethod(preview,"presentFrame",Qt::DirectConnection,
       Q_ARG(QImage,completed),Q_ARG(QRect,previousArea)));
 check(preview->grab().toImage().pixelColor(50,50)==QColor(Qt::blue));
 preview->close(); app.sendPostedEvents(nullptr,QEvent::DeferredDelete); app.processEvents(); check(main.isVisible());
 main.findChild<QPushButton *>("newCaptureButton")->click(); app.processEvents(); selector=main.findChild<CaptureSelector *>();
 QTest::mouseClick(selector,Qt::LeftButton,Qt::NoModifier,QPoint(30,30)); check(selector->isVisible());
 QTest::keyClick(selector,Qt::Key_Right); QTest::keyClick(selector,Qt::Key_Return); app.processEvents();
 preview=main.findChild<CapturePreview *>(); check(preview && preview->isVisible()); preview->close();
 app.sendPostedEvents(nullptr,QEvent::DeferredDelete); app.processEvents(); check(main.isVisible());
}
