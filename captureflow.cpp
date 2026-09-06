#include "captureflow.h"
#include "desktopcapture.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QPushButton>
#include <QRegion>
#include <algorithm>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
class CaptureHandle final : public QPushButton
{
public:
    explicit CaptureHandle(QWidget *parent) : QPushButton(parent) {}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(rect().center(), 14, 14);
        if (hasFocus()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Qt::black, 1, Qt::DotLine));
            p.drawEllipse(rect().center(), 11, 11);
        }
    }
};
constexpr int minimumRegionWidth = 80;
constexpr int minimumRegionHeight = 60;
QPoint globalMouse(QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}
}

CaptureSelector::CaptureSelector(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setObjectName(QStringLiteral("captureSelector"));
    setAccessibleName(tr("Select capture region"));
    setAccessibleDescription(tr("Drag to select. Escape cancels. Arrow keys create and move a selection; Shift and arrow keys resize it. Enter confirms."));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    QRect desktop;
    for (auto *screen : QGuiApplication::screens()) {
        desktop = desktop.united(screen->geometry());
        connect(screen, &QScreen::geometryChanged, this, &QWidget::close);
    }
    connect(qApp, &QGuiApplication::screenAdded, this, &QWidget::close);
    connect(qApp, &QGuiApplication::screenRemoved, this, &QWidget::close);
    setGeometry(desktop);
}

void CaptureSelector::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 65));
    if (!selection.isEmpty()) {
        p.setPen(QPen(Qt::black, 3));
        p.drawRect(selection.adjusted(1, 1, -1, -1));
        p.setPen(QPen(Qt::white, 2, Qt::DotLine));
        p.drawRect(selection.adjusted(1, 1, -1, -1));
    }
}

void CaptureSelector::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    anchor = event->pos();
    dragging = true;
    selection = QRect(anchor, QSize());
    update();
}
void CaptureSelector::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging) return;
    selection = QRect(QPoint(std::min(anchor.x(), event->pos().x()), std::min(anchor.y(), event->pos().y())),
                      QPoint(std::max(anchor.x(), event->pos().x()), std::max(anchor.y(), event->pos().y()))).intersected(rect());
    update();
}
void CaptureSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (!dragging || event->button() != Qt::LeftButton) return;
    selection = QRect(QPoint(std::min(anchor.x(), event->pos().x()), std::min(anchor.y(), event->pos().y())),
                      QPoint(std::max(anchor.x(), event->pos().x()), std::max(anchor.y(), event->pos().y()))).intersected(rect());
    dragging = false;
    finish();
    update();
}
void CaptureSelector::finish()
{
    // Keep tiny/accidental selections in selection mode, rather than spawning unusable chrome.
    if (selection.width() < minimumRegionWidth || selection.height() < minimumRegionHeight) return;
    completed = true;
    hide();
    emit regionSelected(QRect(mapToGlobal(selection.topLeft()), selection.size()));
    close();
}
void CaptureSelector::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) { close(); return; }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) { finish(); return; }
    QPoint delta;
    switch (event->key()) {
    case Qt::Key_Left: delta.setX(-10); break;
    case Qt::Key_Right: delta.setX(10); break;
    case Qt::Key_Up: delta.setY(-10); break;
    case Qt::Key_Down: delta.setY(10); break;
    default: QWidget::keyPressEvent(event); return;
    }
    if (selection.width() < minimumRegionWidth || selection.height() < minimumRegionHeight) {
        selection = QRect(QPoint(), QSize(std::min(320, width()), std::min(200, height())));
        selection.moveCenter(rect().center());
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        selection.setWidth(std::max(minimumRegionWidth, selection.width() + delta.x()));
        selection.setHeight(std::max(minimumRegionHeight, selection.height() + delta.y()));
    } else selection.translate(delta);
    selection = selection.intersected(rect());
    update();
}
void CaptureSelector::closeEvent(QCloseEvent *event)
{
    if (!completed) { completed = true; emit canceled(); }
    QWidget::closeEvent(event);
}

CapturePreview::CapturePreview(const QRect &region, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                      | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus)
{
    setObjectName(QStringLiteral("capturePreview"));
    setWindowTitle(tr("Octans capture test"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    // Controls live in a separate masked window: native input transparency applies
    // to an entire window and cannot be undone for child buttons.
    controls = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    controls->setObjectName(QStringLiteral("captureControls"));
    controls->setAttribute(Qt::WA_TranslucentBackground);
    controls->setAttribute(Qt::WA_ShowWithoutActivating);
    handle = new CaptureHandle(controls);
    handle->setObjectName(QStringLiteral("captureHandle"));
    handle->setAccessibleName(tr("Move capture region"));
    handle->setToolTip(tr("Drag to move; arrow keys move when focused"));
    handle->setCursor(Qt::SizeAllCursor);
    handle->setGeometry(0, 0, 44, 44);
    handle->installEventFilter(this);
    closeButton = new QPushButton(QStringLiteral("×"), controls);
    closeButton->setAccessibleName(tr("Close capture"));
    closeButton->setToolTip(tr("Close capture"));
    closeButton->setStyleSheet(QStringLiteral("QPushButton { color: white; background: transparent; border: 2px solid transparent; font-size: 28px; } QPushButton:focus { border: 2px dashed white; }"));
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    setTabOrder(handle, closeButton);
    setGeometry(region);
    positionControls();
}
CapturePreview::~CapturePreview()
{
    // Stop before QWidget and its native capture/control windows are destroyed.
    delete capture;
}
QRect CapturePreview::physicalRegion() const
{
#ifdef Q_OS_WIN
    const HWND window = reinterpret_cast<HWND>(winId());
    RECT bounds{};
    POINT origin{};
    if (GetClientRect(window, &bounds) && ClientToScreen(window, &origin))
        return QRect(origin.x, origin.y, bounds.right, bounds.bottom);
#endif
    return QRect();
}
void CapturePreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (!frame.isNull()) p.drawImage(rect(), frame);
    else p.fillRect(rect(), QColor(0, 0, 0, 90));
    if (!captureError.isEmpty()) {
        p.setPen(Qt::white);
        p.drawText(rect().adjusted(12, 48, -12, -12), Qt::TextWordWrap, captureError);
    }
}
void CapturePreview::resizeEvent(QResizeEvent *event)
{
    positionControls();
    QWidget::resizeEvent(event);
}
void CapturePreview::positionControls()
{
    controls->setGeometry(x(), y(), width(), 44);
    closeButton->setGeometry(width()-44, 0, 44, 44);
    controls->setMask(QRegion(handle->geometry()).united(QRegion(closeButton->geometry())));
    if (capture) {
        frame = QImage();
        capture->setRegion(physicalRegion());
        update();
    }
}
void CapturePreview::moveEvent(QMoveEvent *event)
{
    positionControls();
    QWidget::moveEvent(event);
}
void CapturePreview::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    positionControls();
    controls->show();
    controls->raise();
    if (!capture && QGuiApplication::platformName() == QStringLiteral("windows")) {
#ifdef Q_OS_WIN
        // Exclude both top-level windows before starting, or never capture at all.
        constexpr DWORD excludeFromCapture = 0x11;
        if (!SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), excludeFromCapture)
            || !SetWindowDisplayAffinity(reinterpret_cast<HWND>(controls->winId()), excludeFromCapture)) {
            captureError = tr("Windows could not exclude Octans from capture. Capture stopped to prevent feedback.");
            setAccessibleDescription(captureError);
            update();
            return;
        }
        capture = new DesktopCapture(this);
        connect(capture, &DesktopCapture::frameReady, this, [this](const QImage &image, const QRect &area) {
            if (area == physicalRegion()) { frame = image; update(); }
            capture->acknowledgeFrame();
        });
        connect(capture, &DesktopCapture::captureFailed, this, [this](const QString &error) {
            captureError = error; frame = QImage();
            setAccessibleDescription(error); update();
        });
        capture->setRegion(physicalRegion());
        capture->start();
#endif
    }
}
void CapturePreview::hideEvent(QHideEvent *event)
{
    controls->hide();
    if (capture) capture->requestInterruption();
    QWidget::hideEvent(event);
}
bool CapturePreview::eventFilter(QObject *object, QEvent *event)
{
    if (object == handle) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                moving = true; dragOffset = globalMouse(mouse) - pos();
            }
        } else if (event->type() == QEvent::MouseMove && moving) {
            move(globalMouse(static_cast<QMouseEvent *>(event)) - dragOffset);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            moving = false;
        } else if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            QPoint delta;
            switch (key->key()) {
            case Qt::Key_Left: delta.setX(-10); break;
            case Qt::Key_Right: delta.setX(10); break;
            case Qt::Key_Up: delta.setY(-10); break;
            case Qt::Key_Down: delta.setY(10); break;
            default: return QWidget::eventFilter(object, event);
            }
            move(pos() + delta); return true;
        }
    }
    return QWidget::eventFilter(object, event);
}
