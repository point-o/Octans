#include "desktopcapture.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

class ColorBoundary final : public QWidget {
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(QRect(0, 0, width()/2, height()), QColor(230,200,40));
        painter.fillRect(QRect(width()/2, 0, width()-width()/2, height()), QColor(220,140,30));
    }
};

class HazardFlowTest : public QObject {
    Q_OBJECT
private slots:
    void analyzesDisplayedColors() {
#ifndef Q_OS_WIN
        QSKIP("Requires native Windows desktop capture");
#else
        if (QApplication::platformName() != QStringLiteral("windows"))
            QSKIP("Requires native Windows platform");
        ColorBoundary source;
        source.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        source.setGeometry(100, 100, 480, 240);
        source.show();
        QVERIFY(QTest::qWaitForWindowExposed(&source));
        QTest::qWait(150);
        RECT bounds{};
        POINT origin{};
        const HWND window = reinterpret_cast<HWND>(source.winId());
        QVERIFY(GetClientRect(window, &bounds));
        QVERIFY(ClientToScreen(window, &origin));
        const QRect area(origin.x, origin.y, bounds.right, bounds.bottom);
        DesktopCapture capture;
        QSignalSpy frames(&capture, &DesktopCapture::frameReady);
        QSignalSpy failures(&capture, &DesktopCapture::captureFailed);
        capture.setRegion(area);
        connect(&capture, &DesktopCapture::frameReady, this,
                [&capture] { capture.acknowledgeFrame(); });
        capture.start();
        QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty() || !failures.isEmpty(), 2000);
        QVERIFY(failures.isEmpty());
        QVERIFY(!frames.isEmpty());
        QVERIFY(qvariant_cast<std::vector<EdgeDetection::Region>>(frames.last().at(2)).empty());
        frames.clear();
        capture.setHazardEnabled(true);
        for (const auto mode : {PixelTransform::Mode::Original, PixelTransform::Mode::Protanopia,
                               PixelTransform::Mode::Deuteranopia, PixelTransform::Mode::Tritanopia}) {
            capture.setSimulationMode(mode);
            QImage expected(2, 1, QImage::Format_RGB32);
            expected.setPixel(0, 0, qRgb(230,200,40));
            expected.setPixel(1, 0, qRgb(220,140,30));
            QVERIFY(PixelTransform::applyInPlace(expected, mode));
            const auto bytes = [](QRgb c) {
                return std::array<quint8,3>{quint8(qRed(c)),quint8(qGreen(c)),quint8(qBlue(c))};
            };
            auto first = bytes(expected.pixel(0,0)), second = bytes(expected.pixel(1,0));
            if (second < first) std::swap(first, second);
            bool found = false;
            QElapsedTimer deadline;
            deadline.start();
            while (!found && deadline.elapsed() < 2500) {
                QTest::qWait(40);
                if (!frames.isEmpty()) {
                    const auto regions = qvariant_cast<std::vector<EdgeDetection::Region>>(frames.last().at(2));
                    for (const auto &region : regions) {
                        if (region.color1 == first && region.color2 == second) {
                            QVERIFY(region.startPixel.x() <= area.width()/2);
                            QVERIFY(region.endPixel.x() >= area.width()/2 - 1);
                            QVERIFY(region.startPixel.y() >= 0);
                            QVERIFY(region.endPixel.y() < area.height());
                            QVERIFY(region.contrastRatio >= 1.0f);
                            QVERIFY(region.contrastRatio < EdgeDetection::DefaultAlarmThreshold);
                            found = true;
                        }
                    }
                    frames.clear(); // Release bounded worker slots.
                }
            }
            QVERIFY2(found, "Expected median colors from the post-simulation frame");
        }
        capture.setHazardEnabled(false);
        frames.clear();
        QTest::qWait(200);
        QVERIFY(!frames.isEmpty());
        QVERIFY(qvariant_cast<std::vector<EdgeDetection::Region>>(frames.last().at(2)).empty());
        capture.requestInterruption();
        QVERIFY(capture.wait(2000));
        QVERIFY(failures.isEmpty());
#endif
    }
};
QTEST_MAIN(HazardFlowTest)
#include "hazardflow_test.moc"
