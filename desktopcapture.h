#ifndef DESKTOPCAPTURE_H
#define DESKTOPCAPTURE_H
#include <QThread>
#include <QImage>
#include <QRect>
#include <QMutex>
#include <atomic>

class DesktopCapture final : public QThread
{
    Q_OBJECT
public:
    explicit DesktopCapture(QObject *parent = nullptr) : QThread(parent) {}
    ~DesktopCapture() override;
    void setRegion(const QRect &physicalRegion);
    void acknowledgeFrame() { pending.store(false); }
signals:
    void frameReady(const QImage &image, const QRect &physicalRegion);
    void captureFailed(const QString &message);
protected:
    void run() override;
private:
    QMutex mutex;
    QRect region;
    std::atomic_bool pending{false};
};
#endif
