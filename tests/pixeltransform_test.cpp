#include "pixeltransform.h"

#include <QtTest>
#include <array>

using PixelTransform::Mode;
using PixelTransform::applyInPlace;

class PixelTransformTest : public QObject {
    Q_OBJECT
private slots:
    void knownColors()
    {
        // Independent double-precision evaluation of the published coefficients
        // with the exact piecewise sRGB transfer function (no production LUT).
        const std::array<QRgb, 5> inputs{{qRgb(255, 0, 0), qRgb(0, 255, 0),
            qRgb(0, 0, 255), qRgb(12, 128, 240), qRgb(200, 80, 25)}};
        const QRgb expected[3][5] = {
            {qRgb(109,95,0), qRgb(255,229,0), qRgb(0,89,255), qRgb(63,139,244), qRgb(115,101,13)},
            {qRgb(163,144,0), qRgb(239,214,58), qRgb(0,61,251), qRgb(0,120,238), qRgb(144,128,18)},
            {qRgb(255,0,15), qRgb(0,247,217), qRgb(0,107,150), qRgb(0,155,172), qRgb(220,49,70)}
        };
        for (int m = 0; m < 3; ++m) {
            QImage image(5, 1, QImage::Format_RGB32);
            for (int i = 0; i < 5; ++i)
                image.setPixel(i, 0, inputs[i]);
            QVERIFY(applyInPlace(image, Mode(m + 1)));
            for (int i = 0; i < 5; ++i) {
                const QRgb pixel = image.pixel(i, 0);
                QVERIFY(qAbs(qRed(pixel) - qRed(expected[m][i])) <= 1);
                QVERIFY(qAbs(qGreen(pixel) - qGreen(expected[m][i])) <= 1);
                QVERIFY(qAbs(qBlue(pixel) - qBlue(expected[m][i])) <= 1);
            }
        }
    }

    void neutralAndAlpha()
    {
        for (const Mode mode : {Mode::Protanopia, Mode::Deuteranopia, Mode::Tritanopia}) {
            QImage image(256, 2, QImage::Format_ARGB32);
            for (int i = 0; i < 256; ++i) {
                image.setPixel(i, 0, qRgba(i, i, i, i));
                image.setPixel(i, 1, qRgba(255, 0, 0, i));
            }
            QVERIFY(applyInPlace(image, mode));
            for (int i = 0; i < 256; ++i) {
                QCOMPARE(image.pixel(i, 0), qRgba(i, i, i, i));
                QCOMPARE(qAlpha(image.pixel(i, 1)), i);
                QCOMPARE(image.pixel(i, 1) & 0xffffff, image.pixel(255, 1) & 0xffffff);
            }
        }
    }

    void storageAndNoOp()
    {
        QImage image(13, 7, QImage::Format_RGB32);
        image.fill(Qt::red);
        image.setDevicePixelRatio(1.5);
        const auto *originalData = image.constBits();
        QVERIFY(applyInPlace(image, Mode::Original));
        QCOMPARE(image.constBits(), originalData);
        QVERIFY(applyInPlace(image, Mode::Protanopia));
        QCOMPARE(image.constBits(), originalData);
        QCOMPARE(image.size(), QSize(13, 7));
        QCOMPARE(image.devicePixelRatio(), 1.5);
        QImage shared = image;
        QVERIFY(applyInPlace(shared, Mode::Original));
        QCOMPARE(shared.constBits(), image.constBits());
        const QRgb saved = image.pixel(0, 0);
        QVERIFY(applyInPlace(shared, Mode::Tritanopia));
        QVERIFY(shared.constBits() != image.constBits());
        QCOMPARE(image.pixel(0, 0), saved);
    }

    void invalidInputs()
    {
        QImage empty;
        QVERIFY(applyInPlace(empty, Mode::Deuteranopia));
        QVERIFY(empty.isNull());
        for (auto format : {QImage::Format_RGB888, QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_Grayscale8, QImage::Format_RGBA8888}) {
            QImage image(3, 2, format);
            image.fill(Qt::red);
            const QImage before = image;
            QVERIFY(!applyInPlace(image, Mode::Protanopia));
            QCOMPARE(image, before);
            QCOMPARE(image.constBits(), before.constBits());
            QVERIFY(applyInPlace(image, Mode::Original));
        }
        QImage image(1, 1, QImage::Format_RGB32);
        image.fill(Qt::green);
        QVERIFY(!applyInPlace(image, Mode(999)));
        QCOMPARE(image.pixel(0, 0), qRgb(0, 255, 0));
    }

    void paddedRows()
    {
        // External buffer has two pixels of padding per row; neither is touched.
        std::array<QRgb, 8> pixels;
        pixels.fill(qRgb(255, 0, 0));
        QImage image(reinterpret_cast<uchar *>(pixels.data()), 2, 2,
                     4 * int(sizeof(QRgb)), QImage::Format_RGB32);
        QVERIFY(applyInPlace(image, Mode::Protanopia));
        QCOMPARE(image.pixel(0, 0), image.pixel(1, 1));
        QVERIFY(image.pixel(0, 0) != qRgb(255, 0, 0));
        for (int i : {2, 3, 6, 7})
            QCOMPARE(pixels[i], qRgb(255, 0, 0));
    }
};

QTEST_GUILESS_MAIN(PixelTransformTest)
#include "pixeltransform_test.moc"
