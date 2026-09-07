#include "edgedetection.h"
#include <QtTest>
#include <array>

class EdgeDetectionTest : public QObject {
    Q_OBJECT
private slots:
    void uniformAndTiny()
    {
        EdgeDetection::Analyzer analyzer;
        for (QSize size : {QSize(1, 1), QSize(2, 8), QSize(20, 15)}) {
            QImage image(size, QImage::Format_RGB32);
            image.fill(Qt::gray);
            QVERIFY(analyzer.analyze(image));
            QCOMPARE(analyzer.size(), size);
            for (const auto &s : analyzer.samples()) {
                QCOMPARE(s.strength, 0.0f);
                QCOMPARE(s.contrast, 1.0f);
            }
        }
        QVERIFY(analyzer.analyze(QImage()));
        QVERIFY(analyzer.samples().empty());
    }

    void stepsAndReuse()
    {
        EdgeDetection::Analyzer analyzer;
        const EdgeDetection::Sample *storage = nullptr;
        for (bool vertical : {true, false}) {
            QImage image(9, 9, QImage::Format_RGB32);
            for (int y = 0; y < 9; ++y)
                for (int x = 0; x < 9; ++x)
                    image.setPixel(x, y, (vertical ? x : y) < 4 ? qRgb(0,0,0) : qRgb(255,255,255));
            const auto *input = image.constBits();
            QVERIFY(analyzer.analyze(image));
            QCOMPARE(input, image.constBits());
            if (storage)
                QCOMPARE(analyzer.samples().data(), storage);
            storage = analyzer.samples().data();
            for (int y = 0; y < 9; ++y) {
                for (int x = 0; x < 9; ++x) {
                    const bool edge = x > 0 && x < 8 && y > 0 && y < 8
                        && ((vertical ? x : y) == 3 || (vertical ? x : y) == 4);
                    const auto &s = analyzer.samples()[y * 9 + x];
                    QVERIFY(qAbs(s.strength - (edge ? 0.70710678f : 0.0f)) < 0.00001f);
                    QVERIFY(qAbs(s.contrast - (edge ? 21.0f : 1.0f)) < 0.00001f);
                }
            }
        }
    }

    void alphaFormatsAndStride()
    {
        EdgeDetection::Analyzer analyzer;
        std::array<QRgb, 35> pixels;
        pixels.fill(qRgba(0,0,0,0));
        QImage image(reinterpret_cast<uchar *>(pixels.data()), 5, 5, 7 * sizeof(QRgb), QImage::Format_ARGB32);
        QVERIFY(analyzer.analyze(image));
        for (const auto &s : analyzer.samples())
            QCOMPARE(s.contrast, 1.0f); // All transparent pixels composite to white.
        image.setPixel(3, 2, qRgba(0,0,0,128));
        QVERIFY(analyzer.analyze(image));
        const float expected = 1.05f / (1.0f - 128.0f / 255.0f + 0.05f);
        QVERIFY(qAbs(analyzer.samples()[12].contrast - expected) < 0.00001f);
        const auto *saved = analyzer.samples().data();
        for (auto format : {QImage::Format_RGB888, QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA8888, QImage::Format_Grayscale8}) {
            QVERIFY(!analyzer.analyze(QImage(4, 4, format)));
            QCOMPARE(analyzer.size(), QSize(5, 5));
            QCOMPARE(analyzer.samples().data(), saved);
        }
        for (int y = 0; y < 5; ++y) {
            QCOMPARE(pixels[y * 7 + 5], qRgba(0,0,0,0));
            QCOMPARE(pixels[y * 7 + 6], qRgba(0,0,0,0));
        }
    }

    void linearLuminance()
    {
        QImage image(5, 5, QImage::Format_RGB32);
        image.fill(Qt::black);
        image.setPixel(3, 2, qRgb(128,128,128));
        EdgeDetection::Analyzer analyzer;
        QVERIFY(analyzer.analyze(image));
        // Independent reference: sRGB 128 has linear luminance 0.2158605.
        QVERIFY(qAbs(analyzer.samples()[12].contrast - 5.317210f) < 0.0001f);
    }

    void diagonal()
    {
        QImage image(7, 7, QImage::Format_RGB32);
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 7; ++x)
                image.setPixel(x, y, x + y < 6 ? qRgb(0,0,0) : qRgb(255,255,255));
        EdgeDetection::Analyzer analyzer;
        QVERIFY(analyzer.analyze(image));
        QVERIFY(qAbs(analyzer.samples()[3 * 7 + 3].strength - 1.0f) < 0.00001f);
        QVERIFY(qAbs(analyzer.samples()[3 * 7 + 3].contrast - 21.0f) < 0.00001f);
    }

    void benchmark_data()
    {
        QTest::addColumn<QSize>("size");
        QTest::newRow("720p") << QSize(1280, 720);
        QTest::newRow("1080p") << QSize(1920, 1080);
    }
    void benchmark()
    {
        QFETCH(QSize, size);
        QImage image(size, QImage::Format_RGB32);
        for (int y = 0; y < size.height(); ++y)
            for (int x = 0; x < size.width(); ++x)
                image.setPixel(x,y,qRgb(x % 256,y % 256,(x + y) % 256));
        EdgeDetection::Analyzer analyzer;
        QVERIFY(analyzer.analyze(image));
        QBENCHMARK { analyzer.analyze(image); }
    }
};
QTEST_GUILESS_MAIN(EdgeDetectionTest)
#include "edgedetection_test.moc"
