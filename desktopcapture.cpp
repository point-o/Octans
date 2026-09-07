#include "desktopcapture.h"
#include <QMutexLocker>
#include <QElapsedTimer>
#include <array>
#include <cstring>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

DesktopCapture::~DesktopCapture()
{
    requestInterruption();
    changed.wakeAll();
    wait();
}
void DesktopCapture::acknowledgeFrame()
{
    QMutexLocker lock(&mutex);
    pending.store(false);
    changed.wakeAll();
}
void DesktopCapture::setRegion(const QRect &physicalRegion)
{
    QMutexLocker lock(&mutex);
    region = physicalRegion;
    changed.wakeAll();
}
void DesktopCapture::run()
{
#ifdef Q_OS_WIN
    // All GDI resources are owned and released on this worker thread.
    const HDC screen = GetDC(nullptr);
    const HDC memory = screen ? CreateCompatibleDC(screen) : nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ original = nullptr;
    void *bits = nullptr;
    QSize allocated;
    QString failure;
    // Owned slots survive signal delivery. Never write a slot while a queued
    // signal, pixmap, or consumer still shares its storage; skip instead of
    // allocating unbounded replacement images for a slow consumer.
    std::array<QImage, 3> frames;
    QElapsedTimer cadence;
    cadence.start();
    constexpr qint64 framePeriodNs = 1000000000 / 60;
    qint64 nextFrameNs = 0;
    if (!memory) failure = tr("Desktop capture could not open the display.");
    while (failure.isEmpty() && !isInterruptionRequested()) {
        QRect area;
        {
            QMutexLocker lock(&mutex);
            area = region;
            const qint64 remainingNs = nextFrameNs - cadence.nsecsElapsed();
            if (area.isEmpty() || pending.load() || remainingNs > 0) {
                // Bounded wait also observes requestInterruption() from callers.
                const unsigned long delayMs = remainingNs > 0
                    ? static_cast<unsigned long>((remainingNs + 999999) / 1000000)
                    : 20;
                changed.wait(&mutex, delayMs);
                continue;
            }
        }
        nextFrameNs = cadence.nsecsElapsed() + framePeriodNs;
        QImage *available = nullptr;
        for (QImage &candidate : frames) {
            if (candidate.isNull() || candidate.isDetached()) {
                available = &candidate;
                break;
            }
        }
        if (!available) continue;
        QImage &frame = *available;
        if (frame.size() != area.size())
            frame = QImage(area.size(), QImage::Format_RGB32);
        if (frame.isNull()) { failure = tr("Desktop capture ran out of image memory."); break; }
        if (allocated != area.size()) {
            if (bitmap) { SelectObject(memory, original); DeleteObject(bitmap); bitmap = nullptr; }
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = area.width();
            info.bmiHeader.biHeight = -area.height();
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!bitmap) { failure = tr("Desktop capture could not allocate its image buffer."); break; }
            original = SelectObject(memory, bitmap);
            if (!original || original == HGDI_ERROR) { failure = tr("Desktop capture could not select its image buffer."); break; }
            allocated = area.size();
        }
        if (!BitBlt(memory, 0, 0, area.width(), area.height(), screen,
                    area.x(), area.y(), SRCCOPY | CAPTUREBLT)) {
            failure = tr("Desktop capture could not read this region."); break;
        }
        GdiFlush();
        // A single copy separates the reusable DIB from asynchronous readers.
        // Both RGB32 layouts are tightly packed; storage is reused at steady size.
        std::memcpy(frame.bits(), bits, size_t(frame.sizeInBytes()));
        if (!PixelTransform::applyInPlace(frame, mode.load())) {
            failure = tr("The selected pixel transform could not process this image."); break;
        }
        pending.store(true);
        emit frameReady(frame, area);
    }
    if (bitmap) {
        if (original && original != HGDI_ERROR) SelectObject(memory, original);
        DeleteObject(bitmap);
    }
    if (memory) DeleteDC(memory);
    if (screen) ReleaseDC(nullptr, screen);
    if (!failure.isEmpty()) emit captureFailed(failure);
#else
    emit captureFailed(tr("Live desktop capture currently requires Windows."));
#endif
}
