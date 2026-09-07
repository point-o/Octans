#ifndef DESKTOPCAPTURE_H
#define DESKTOPCAPTURE_H
#include <QThread>
#include <QImage>
#include <QRect>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include "pixeltransform.h"

class DesktopCapture final : public QThread
{
    Q_OBJECT
public:
    explicit DesktopCapture(QObject *parent = nullptr) : QThread(parent) {}
    ~DesktopCapture() override;
    void setRegion(const QRect &physicalRegion);
    void acknowledgeFrame();
    void setSimulationMode(PixelTransform::Mode value) { mode.store(value); }
signals:
    void frameReady(const QImage &image, const QRect &physicalRegion);
    void captureFailed(const QString &message);
protected:
    void run() override;
private:
    QMutex mutex;
    QWaitCondition changed;
    QRect region;
    std::atomic_bool pending{false};
    std::atomic<PixelTransform::Mode> mode{PixelTransform::Mode::Original};
};
#endif
