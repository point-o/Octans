#include "desktopcapture.h"
#include <QSignalSpy>
#include <QTest>

class DesktopCaptureTest : public QObject
{
    Q_OBJECT
private slots:
    void boundedStorageAndBackpressure()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows GDI capture test");
#else
        DesktopCapture capture;
        QSignalSpy failures(&capture, &DesktopCapture::captureFailed);
        QSignalSpy frames(&capture, &DesktopCapture::frameReady);
        capture.setRegion(QRect(0, 0, 64, 64));
        capture.start();
        QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty() || !failures.isEmpty(), 2000);
        QVERIFY2(failures.isEmpty(), failures.isEmpty() ? "" : qPrintable(failures.first().first().toString()));
        QTRY_COMPARE_WITH_TIMEOUT(frames.size(), 1, 2000);
        QTest::qWait(100);
        QCOMPARE(frames.size(), 1); // No acknowledgement: no queued backlog.
        const QImage first = qvariant_cast<QImage>(frames.at(0).at(0));
        const QImage snapshot = first.copy();
        capture.acknowledgeFrame();
        QTRY_COMPARE_WITH_TIMEOUT(frames.size(), 2, 2000);
        capture.acknowledgeFrame();
        QTRY_COMPARE_WITH_TIMEOUT(frames.size(), 3, 2000);
        QImage second = qvariant_cast<QImage>(frames.at(1).at(0));
        QImage third = qvariant_cast<QImage>(frames.at(2).at(0));
        QVERIFY(first.constBits() != second.constBits());
        QVERIFY(second.constBits() != third.constBits());
        QVERIFY(first.constBits() != third.constBits());
        capture.acknowledgeFrame();
        QTest::qWait(100);
        QCOMPARE(frames.size(), 3); // All three slots retained: drop, don't allocate.
        QCOMPARE(first, snapshot);
        // Releasing readers makes a slot available without another acknowledgement.
        frames.clear();
        second = QImage();
        third = QImage();
        capture.setRegion(QRect(10, 10, 80, 48));
        QTRY_COMPARE_WITH_TIMEOUT(frames.size(), 1, 2000);
        QCOMPARE(qvariant_cast<QImage>(frames.at(0).at(0)).size(), QSize(80, 48));
        QCOMPARE(first, snapshot);
        capture.requestInterruption();
        QVERIFY(capture.wait(1000));
        QVERIFY(failures.isEmpty());
#endif
    }
    void reuseAndResize()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows GDI capture test");
#else
        DesktopCapture capture;
        QSignalSpy failures(&capture, &DesktopCapture::captureFailed);
        QSignalSpy frames(&capture, &DesktopCapture::frameReady);
        capture.setRegion(QRect(0, 0, 64, 64));
        capture.start();
        QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty() || !failures.isEmpty(), 2000);
        QVERIFY2(failures.isEmpty(), failures.isEmpty() ? "" : qPrintable(failures.first().first().toString()));
        QTRY_COMPARE_WITH_TIMEOUT(frames.size(), 1, 2000);
        const uchar *storage = qvariant_cast<QImage>(frames.at(0).at(0)).constBits();
        frames.clear();
        capture.acknowledgeFrame();
        QTRY_COMPARE_WITH_TIMEOUT(frames.size(), 1, 2000);
        QCOMPARE(qvariant_cast<QImage>(frames.at(0).at(0)).constBits(), storage);
        frames.clear();
        const QRect resized(10, 10, 96, 80);
        capture.setRegion(resized);
        capture.acknowledgeFrame();
        QTRY_COMPARE_WITH_TIMEOUT(frames.size(), 1, 2000);
        QCOMPARE(qvariant_cast<QImage>(frames.at(0).at(0)).size(), resized.size());
        QCOMPARE(frames.at(0).at(1).toRect(), resized);
        capture.requestInterruption();
        QVERIFY(capture.wait(1000));
        QVERIFY(failures.isEmpty());
#endif
    }
};
QTEST_GUILESS_MAIN(DesktopCaptureTest)
#include "desktopcapture_test.moc"

