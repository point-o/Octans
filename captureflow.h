#ifndef CAPTUREFLOW_H
#define CAPTUREFLOW_H
#include <QWidget>
#include <QPushButton>
#include <QPoint>
#include <QImage>
#include <QElapsedTimer>
#include "pixeltransform.h"
class QKeyEvent;
class QShowEvent;
class QHideEvent;
class DesktopCapture;

class CaptureHandle final : public QPushButton
{
    Q_OBJECT
public:
    explicit CaptureHandle(QWidget *parent = nullptr);
    void setMode(PixelTransform::Mode mode) { currentMode = mode; update(); }
    PixelTransform::Mode mode() const { return currentMode; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    PixelTransform::Mode currentMode = PixelTransform::Mode::Original;
};

class ModeCircleButton final : public QPushButton
{
    Q_OBJECT
public:
    explicit ModeCircleButton(QWidget *parent = nullptr);
    void setCircle(int diameter, char letter, PixelTransform::Mode mode,
                   const QString &name, const QString &tip);
    void setActive(bool value) { active = value; update(); }
    PixelTransform::Mode mode() const { return mode_; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QChar letter;
    PixelTransform::Mode mode_ = PixelTransform::Mode::Original;
    bool active = false;
};

class ModePopup final : public QWidget
{
    Q_OBJECT
public:
    explicit ModePopup(QWidget *parent = nullptr);
    void setMode(PixelTransform::Mode mode);
    void closeFromToggle();
    bool consumeExternalDismissal();
signals:
    void modeChosen(PixelTransform::Mode mode);
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    void dismissAsSelf();
    void chooseFrom(ModeCircleButton *button);
    void moveFocus(int step);
    PixelTransform::Mode currentMode = PixelTransform::Mode::Original;
    ModeCircleButton *circles[3] = {nullptr, nullptr, nullptr};
    bool closingSelf = false;
    bool externallyClosed = false;
    QElapsedTimer externalTimer;
};

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
    void setSimulationMode(PixelTransform::Mode mode);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void moveEvent(QMoveEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
private:
    Q_SLOT void presentFrame(const QImage &image, const QRect &area);
    void positionControls();
    QRect physicalRegion() const;
    DesktopCapture *capture = nullptr;
    QImage frame;
    QString captureError;
PixelTransform::Mode simulationMode = PixelTransform::Mode::Original;
    QWidget *controls;
    CaptureHandle *handle;
    ModePopup *modePopup = nullptr;
    QPushButton *closeButton;
    QPushButton *resizeHandle = nullptr;
    QSize resizeStart;
    QPoint resizePress;
    bool resizing = false;
    QPoint dragOffset;
    QPoint pressPosition;
    bool dragged = false;
    bool moving = false;
};
#endif
