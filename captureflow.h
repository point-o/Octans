#ifndef CAPTUREFLOW_H
#define CAPTUREFLOW_H
#include <QWidget>
#include <QPoint>
#include <QImage>
class QPushButton;
class DesktopCapture;

class CaptureSelector final : public QWidget
{
    Q_OBJECT
public:
    explicit CaptureSelector(QWidget *parent = nullptr);
signals:
    void regionSelected(const QRect &globalRegion);
    void canceled();
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void closeEvent(QCloseEvent *) override;
private:
    void finish();
    QPoint anchor;
    QRect selection;
    bool dragging = false;
    bool completed = false;
};

class CapturePreview final : public QWidget
{
    Q_OBJECT
public:
    explicit CapturePreview(const QRect &region, QWidget *parent = nullptr);
    ~CapturePreview() override;
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void moveEvent(QMoveEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
private:
    void positionControls();
    QRect physicalRegion() const;
    DesktopCapture *capture = nullptr;
    QImage frame;
    QString captureError;
    QWidget *controls;
    QPushButton *handle;
    QPushButton *closeButton;
    QPoint dragOffset;
    bool moving = false;
};
#endif
