#include "edgedetection.h"
#include <QtTest>
#include <array>

class EdgeDetectionTest : public QObject {
    Q_OBJECT
private slots:
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
            QVERIFY(!region.indeterminate);
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

    void regionsReliability()
    {
        EdgeDetection::RegionAnalyzer analyzer;
        // Solid black-on-white boundary: both sides have perfect color anchors.
        QImage solid(20, 20, QImage::Format_RGB32);
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x)
                solid.setPixel(x, y, x < 10 ? qRgb(0,0,0) : qRgb(255,255,255));
        QVERIFY(analyzer.analyze(solid));
        QCOMPARE(analyzer.regions().size(), size_t(1));
        QVERIFY(!analyzer.regions()[0].indeterminate);
        QCOMPARE(analyzer.regions()[0].contrastRatio, 21.0f);
        // Wide luminance ramp: both sides are blend pixels with no anchor.
        QImage ramp(40, 20, QImage::Format_RGB32);
        for (int y = 0; y < ramp.height(); ++y)
            for (int x = 0; x < ramp.width(); ++x)
                ramp.setPixel(x, y, qRgb(x < 12 ? 255 : (x < 27 ? 255 - (x - 11) * 17 : 255),
                                         0, 0));
        QVERIFY(analyzer.analyze(ramp));
        QCOMPARE(analyzer.regions().size(), size_t(1));
        QVERIFY(analyzer.regions()[0].indeterminate);
        // Narrow antialiased core still keeps a solid interior: a 3px gray stem
        // (60) with single-pixel 150 ramps on a 255 background confirms at the
        // 0.5 share, so its medians reflect the core and the ramp midpoint,
        // not pure noise.
        QImage core(40, 20, QImage::Format_RGB32);
        for (int y = 0; y < core.height(); ++y)
            for (int x = 0; x < core.width(); ++x) {
                const int value = x < 13 ? 255 : x < 14 ? 150 : x < 17 ? 60
                    : x < 18 ? 150 : 255;
                core.setPixel(x, y, qRgb(value, 0, 0));
            }
        QVERIFY(analyzer.analyze(core));
        QVERIFY(!analyzer.regions().empty());
        const auto &r = analyzer.regions()[0];
        QVERIFY(!r.indeterminate);
        QCOMPARE(r.color1, (std::array<quint8,3>{60,0,0}));
        QCOMPARE(r.color2, (std::array<quint8,3>{150,0,0}));
        // Storage reuse: the region vector pointer stays stable across frames.
        const auto *storage = analyzer.regions().data();
        QVERIFY(analyzer.analyze(core));
        QCOMPARE(analyzer.regions().data(), storage);
    }

    void severityClassifier()
    {
        constexpr float t = EdgeDetection::DefaultAlarmThreshold;
        QCOMPARE(EdgeDetection::severityFor(0.8f, false, t), EdgeDetection::Severity::Critical);
        QCOMPARE(EdgeDetection::severityFor(1.0f, false, t), EdgeDetection::Severity::Warn);
        QCOMPARE(EdgeDetection::severityFor(1.9f, false, t), EdgeDetection::Severity::Warn);
        QCOMPARE(EdgeDetection::severityFor(2.0f, false, t), EdgeDetection::Severity::Marginal);
        QCOMPARE(EdgeDetection::severityFor(2.9f, false, t), EdgeDetection::Severity::Marginal);
        QCOMPARE(EdgeDetection::severityFor(3.0f, false, t), EdgeDetection::Severity::Hidden);
        QCOMPARE(EdgeDetection::severityFor(4.4f, false, 4.5f), EdgeDetection::Severity::Warn);
        QCOMPARE(EdgeDetection::severityFor(4.5f, false, 4.5f), EdgeDetection::Severity::Hidden);
        QCOMPARE(EdgeDetection::severityFor(0.5f, true, t), EdgeDetection::Severity::Indeterminate);
        QCOMPARE(EdgeDetection::severityFor(9.0f, true, 4.5f), EdgeDetection::Severity::Indeterminate);
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