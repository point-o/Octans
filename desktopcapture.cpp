#include "desktopcapture.h"
#include <QMutexLocker>
#include <QElapsedTimer>
#include <array>
#include <cstring>
#include <algorithm>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

DesktopCapture::DesktopCapture(QObject *parent) : QThread(parent)
{
    qRegisterMetaType<std::vector<EdgeDetection::Region>>();
}
void DesktopCapture::setSimulationMode(PixelTransform::Mode value)
{
    if (mode.exchange(value) != value) analysisRevision.fetch_add(1);
    changed.wakeAll();
}
void DesktopCapture::setHazardEnabled(bool enabled)
{
    if (hazardEnabled.exchange(enabled) != enabled) analysisRevision.fetch_add(1);
    changed.wakeAll();
}
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
    if (region != physicalRegion) {
        region = physicalRegion;
        analysisRevision.fetch_add(1);
    }
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
    EdgeDetection::RegionAnalyzer analyzer;
    QImage analysisImage;
    std::vector<EdgeDetection::Region> regions;
    QRect analyzedArea;
    PixelTransform::Mode analyzedMode = PixelTransform::Mode::Original;
    quint64 analyzedRevision = 0;
    qint64 nextAnalysisNs = 0;
    constexpr qint64 analysisPeriodNs = 100000000; // At most 10 analyses/second.
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
        const quint64 revision = analysisRevision.load();
        const auto frameMode = mode.load();
        const bool analyzeHazards = hazardEnabled.load();
        if (!PixelTransform::applyInPlace(frame, frameMode)) {
            failure = tr("The selected pixel transform could not process this image."); break;
        }
        if (!analyzeHazards || analyzedArea != area || analyzedMode != frameMode
            || analyzedRevision != revision) {
            regions.clear();
            analyzedArea = area;
            analyzedMode = frameMode;
            analyzedRevision = revision;
        }
        if (analyzeHazards && cadence.nsecsElapsed() >= nextAnalysisNs) {
            nextAnalysisNs = cadence.nsecsElapsed() + analysisPeriodNs;
            QSize sampleSize = frame.size();
            if (sampleSize.width() > 640 || sampleSize.height() > 360) {
                sampleSize.scale(QSize(640, 360), Qt::KeepAspectRatio);
                sampleSize.setWidth(std::max(1, sampleSize.width()));
                sampleSize.setHeight(std::max(1, sampleSize.height()));
            }
            if (analysisImage.size() != sampleSize)
                analysisImage = QImage(sampleSize, QImage::Format_RGB32);
            if (analysisImage.isNull()) {
                failure = tr("Desktop capture could not allocate its hazard buffer.");
                break;
            }
            // Nearest samples preserve exact transformed bytes; no filtered colors
            // are introduced before the color-region comparison.
            for (int y = 0; y < sampleSize.height(); ++y) {
                const int sourceY = int(qint64(y) * frame.height() / sampleSize.height());
                const auto *source = reinterpret_cast<const QRgb *>(frame.constScanLine(sourceY));
                auto *destination = reinterpret_cast<QRgb *>(analysisImage.scanLine(y));
                for (int x = 0; x < sampleSize.width(); ++x)
                    destination[x] = source[qint64(x) * frame.width() / sampleSize.width()];
            }
            regions.clear();
            if (analyzer.analyze(analysisImage)) {
                regions.assign(analyzer.regions().begin(), analyzer.regions().end());
                for (auto &hazard : regions) {
                    const auto start = [](int value, int full, int sampled) {
                        return std::clamp(int(qint64(value) * full / sampled), 0, full - 1);
                    };
                    const auto end = [](int value, int full, int sampled) {
                        return std::clamp(int((qint64(value + 1) * full + sampled - 1)
                                              / sampled - 1), 0, full - 1);
                    };
                    hazard.startPixel = QPoint(start(hazard.startPixel.x(), frame.width(), sampleSize.width()),
                                               start(hazard.startPixel.y(), frame.height(), sampleSize.height()));
                    hazard.endPixel = QPoint(end(hazard.endPixel.x(), frame.width(), sampleSize.width()),
                                             end(hazard.endPixel.y(), frame.height(), sampleSize.height()));
                }
            }
        }
        // A change during analysis invalidates annotations for this queued frame.
        if (analysisRevision.load() != revision || !hazardEnabled.load()) regions.clear();
        pending.store(true);
        emit frameReady(frame, area, regions);
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
