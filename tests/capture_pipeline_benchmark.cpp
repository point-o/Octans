#include "pixeltransform.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <cstring>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QFile output(app.arguments().value(1, "capture-pipeline-benchmark.txt"));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream report(&output);
    report << "Copy + in-place transform; preallocated RGB32 buffers; excludes GDI/UI.\n"
              "Release build, 5 warmups + 30 measured frames per case; no baseline comparison.\n";
    const char *names[] = {"Original", "Protanopia", "Deuteranopia", "Tritanopia"};
    for (const QSize size : {QSize(640, 480), QSize(1920, 1080)}) {
        QImage source(size, QImage::Format_RGB32);
        QImage frame(size, QImage::Format_RGB32);
        for (int y = 0; y < size.height(); ++y) {
            auto *row = reinterpret_cast<QRgb *>(source.scanLine(y));
            for (int x = 0; x < size.width(); ++x)
                row[x] = qRgb((x * 13 + y) % 256, (y * 7 + x) % 256, (x + y * 3) % 256);
        }
        uchar *const storage = frame.bits();
        for (int mode = 0; mode != 4; ++mode) {
            auto process = [&] {
                std::memcpy(frame.bits(), source.constBits(), size_t(source.sizeInBytes()));
                return PixelTransform::applyInPlace(frame, static_cast<PixelTransform::Mode>(mode));
            };
            for (int i = 0; i != 5; ++i) if (!process()) return 2;
            QElapsedTimer timer;
            timer.start();
            for (int i = 0; i != 30; ++i) if (!process()) return 2;
            const double meanMs = timer.nsecsElapsed() / 30000000.0;
            if (frame.constBits() != storage) return 3;
            report << size.width() << 'x' << size.height() << ' ' << names[mode]
                   << ": " << meanMs << " ms/frame; storage reused\n";
        }
    }
    return 0;
}
