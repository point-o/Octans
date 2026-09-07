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

    void regionsFlatNoiseAndSeparate()
    {
        EdgeDetection::RegionAnalyzer analyzer;
        QImage image(40, 30, QImage::Format_RGB32);
        image.fill(Qt::gray);
        QVERIFY(analyzer.analyze(image));
        QVERIFY(analyzer.regions().empty());
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x, y, qRgb(125 + (x + y) % 6, 128, 128));
        QVERIFY(analyzer.analyze(image));
        QVERIFY(analyzer.regions().empty());
        image.fill(Qt::black);
        image.setPixel(4, 4, qRgb(255,255,255)); // Three anchors: isolated speck rejected.
        QVERIFY(analyzer.analyze(image));
        QVERIFY(analyzer.regions().empty());
        image.fill(Qt::black);
        for (QRect rectangle : {QRect(3, 4, 6, 7), QRect(25, 19, 6, 7)})
            for (int y = rectangle.top(); y <= rectangle.bottom(); ++y)
                for (int x = rectangle.left(); x <= rectangle.right(); ++x)
                    image.setPixel(x, y, qRgb(255,255,255));
        const QImage shared = image;
        QVERIFY(analyzer.analyze(image));
        QCOMPARE(image.constBits(), shared.constBits());
        QCOMPARE(analyzer.regions().size(), size_t(2));
        QCOMPARE(analyzer.regions()[0].startPixel, QPoint(2,3));
        QCOMPARE(analyzer.regions()[0].endPixel, QPoint(9,11));
        for (const auto &region : analyzer.regions()) {
            QCOMPARE(region.color1, (std::array<quint8,3>{0,0,0}));
            QCOMPARE(region.color2, (std::array<quint8,3>{255,255,255}));
            QCOMPARE(region.contrastRatio, 21.0f);
        }
        const auto *storage = analyzer.regions().data();
        QVERIFY(analyzer.analyze(image));
        QCOMPARE(analyzer.regions().data(), storage);
    }

    void regionsColorsAndOrientation()
    {
        EdgeDetection::RegionAnalyzer analyzer;
        for (int orientation = 0; orientation < 4; ++orientation) {
            QImage image(20, 20, QImage::Format_RGB32);
            for (int y = 0; y < 20; ++y) {
                for (int x = 0; x < 20; ++x) {
                    const bool first = orientation == 0 ? x < 10 : orientation == 1 ? y < 10
                        : orientation == 2 ? x + y < 20 : x < y;
                    image.setPixel(x, y, first ? qRgb(255,165,0) : qRgb(255,255,0));
                }
            }
            QVERIFY(analyzer.analyze(image));
            QCOMPARE(analyzer.regions().size(), size_t(1));
            const auto &r = analyzer.regions()[0];
            QCOMPARE(r.color1, (std::array<quint8,3>{255,165,0}));
            QCOMPARE(r.color2, (std::array<quint8,3>{255,255,0}));
            QVERIFY(qAbs(r.contrastRatio - 1.837f) < 0.002f);
            QVERIFY(image.rect().contains(r.startPixel));
            QVERIFY(image.rect().contains(r.endPixel));
        }
        // Similar linear luminance, very different hues: should still be found.
        QImage image(12, 12, QImage::Format_RGB32);
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < 12; ++x)
                image.setPixel(x,y,x < 6 ? qRgb(255,0,0) : qRgb(0,148,0));
        QVERIFY(analyzer.analyze(image));
        QCOMPARE(analyzer.regions().size(), size_t(1));
        QVERIFY(analyzer.regions()[0].contrastRatio < 1.02f);
    }

    void regionsRawMediansAndBreakpoint()
    {
        EdgeDetection::RegionAnalyzer analyzer;
        QImage image(12, 9, QImage::Format_RGB32);
        // Small variations stay below edge threshold. Independent channel
        // medians are 255/165/2 and 255/255/2; colors aren't averaged/linearized.
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 12; ++x)
                image.setPixel(x,y,x < 6 ? qRgb(255,161+y,y%5) : qRgb(255,255,y%5));
        QVERIFY(analyzer.analyze(image));
        QCOMPARE(analyzer.regions().size(), size_t(1));
        QCOMPARE(analyzer.regions()[0].color1, (std::array<quint8,3>{255,165,2}));
        QCOMPARE(analyzer.regions()[0].color2, (std::array<quint8,3>{255,255,2}));
        for (int low : {10, 11}) {
            for (int y = 0; y < 9; ++y)
                for (int x = 0; x < 12; ++x)
                    image.setPixel(x,y,x < 6 ? qRgb(low,0,0) : qRgb(22,0,0));
            QVERIFY(analyzer.analyze(image));
            // 10->22 meets the inclusive 12-byte threshold; 11->22 does not.
            QCOMPARE(analyzer.regions().size(), low == 10 ? size_t(1) : size_t(0));
            if (low == 10)
                QVERIFY(qAbs(analyzer.regions()[0].contrastRatio - 1.021f) < 0.002f);
        }
    }

    void regionsLimitsFormatsAndStride()
    {
        EdgeDetection::RegionAnalyzer analyzer;
        std::array<QRgb, 7 * 8> pixels;
        pixels.fill(qRgb(255,0,0));
        QImage padded(reinterpret_cast<uchar *>(pixels.data()), 5, 8, 7 * sizeof(QRgb), QImage::Format_RGB32);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 5; ++x)
                padded.setPixel(x,y,x < 2 ? qRgb(0,0,0) : qRgb(255,255,255));
        QVERIFY(analyzer.analyze(padded));
        QCOMPARE(analyzer.regions().size(), size_t(1));
        QCOMPARE(analyzer.regions()[0].startPixel, QPoint(1,0));
        QCOMPARE(analyzer.regions()[0].endPixel, QPoint(2,7));
        for (int y = 0; y < 8; ++y)
            QCOMPARE(pixels[y * 7 + 5], qRgb(255,0,0));
        for (const QImage &invalid : {QImage(10,10,QImage::Format_ARGB32),
                QImage(10,10,QImage::Format_RGB888), QImage(4097,1,QImage::Format_RGB32),
                QImage(1025,1024,QImage::Format_RGB32)}) {
            QVERIFY(!analyzer.analyze(invalid));
            QCOMPARE(analyzer.regions().size(), size_t(1));
        }
        QImage many(320,320,QImage::Format_RGB32);
        many.fill(Qt::black);
        for (int y = 3; y < 318; y += 8)
            for (int x = 3; x < 318; x += 8)
                for (int oy = 0; oy < 2; ++oy)
                    for (int ox = 0; ox < 2; ++ox)
                        many.setPixel(x+ox,y+oy,qRgb(255,255,255));
        QVERIFY(analyzer.analyze(many));
        QCOMPARE(analyzer.regions().size(), EdgeDetection::RegionAnalyzer::MaximumRegions);
        QVERIFY(analyzer.analyze(QImage()));
        QVERIFY(analyzer.regions().empty());
    }

    void regionBenchmark()
    {
        QImage image(640,360,QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x,y,((x / 32 + y / 32) % 2) ? qRgb(255,165,0) : qRgb(255,255,0));
        EdgeDetection::RegionAnalyzer analyzer;
        QVERIFY(analyzer.analyze(image));
        QBENCHMARK { analyzer.analyze(image); }
    }
};
QTEST_GUILESS_MAIN(EdgeDetectionTest)
#include "edgedetection_test.moc"
