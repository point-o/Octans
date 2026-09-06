#include "desktopcapture.h"
#include <QMutexLocker>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

DesktopCapture::~DesktopCapture()
{
    requestInterruption();
    wait();
}
void DesktopCapture::setRegion(const QRect &physicalRegion)
{
    QMutexLocker lock(&mutex);
    region = physicalRegion;
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
    if (!memory) failure = tr("Desktop capture could not open the display.");
    while (failure.isEmpty() && !isInterruptionRequested()) {
        QRect area;
        { QMutexLocker lock(&mutex); area = region; }
        if (area.isEmpty() || pending.load()) { msleep(20); continue; }
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
        // Copy only the selected region. One queued image maximum; the DIB is reused.
        QImage frame = QImage(static_cast<const uchar *>(bits), area.width(), area.height(),
                                    area.width() * 4, QImage::Format_RGB32).copy();
        if (frame.isNull()) { failure = tr("Desktop capture ran out of image memory."); break; }
        if (!PixelTransform::applyInPlace(frame, mode.load())) {
            failure = tr("The selected pixel transform could not process this image."); break;
        }
        pending.store(true);
        emit frameReady(frame, area);
        msleep(100); // Modest 10 FPS prototype, independent of UI event handling.
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
