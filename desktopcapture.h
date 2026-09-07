#ifndef DESKTOPCAPTURE_H
#define DESKTOPCAPTURE_H
#include <QThread>
#include <QImage>
#include <QRect>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include "pixeltransform.h"
#include "edgedetection.h"

Q_DECLARE_METATYPE(std::vector<EdgeDetection::Region>)

class DesktopCapture final : public QThread
{
    Q_OBJECT
public:
    explicit DesktopCapture(QObject *parent = nullptr);
    ~DesktopCapture() override;
    void setRegion(const QRect &physicalRegion);
    void acknowledgeFrame();
    void setSimulationMode(PixelTransform::Mode value);
    void setHazardEnabled(bool enabled);
signals:
    void frameReady(const QImage &image, const QRect &physicalRegion,
                    const std::vector<EdgeDetection::Region> &regions);
    void captureFailed(const QString &message);
protected:
    void run() override;
private:
    QMutex mutex;
    QWaitCondition changed;
    QRect region;
    std::atomic_bool pending{false};
    std::atomic<PixelTransform::Mode> mode{PixelTransform::Mode::Original};
    std::atomic_bool hazardEnabled{false};
    std::atomic<quint64> analysisRevision{0};
};
#endif
