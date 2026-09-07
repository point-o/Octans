#include "captureflow.h"
#include "desktopcapture.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QPushButton>
#include <QRegion>
#include <QPainterPath>
#include <algorithm>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
class CaptureCloseButton final : public QPushButton
{
public:
    explicit CaptureCloseButton(QWidget *parent) : QPushButton(parent) {
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::StrongFocus);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF center = QRectF(rect()).center();
        QPainterPath cross;
        cross.moveTo(center + QPointF(-7, -7));
        cross.lineTo(center + QPointF(7, 7));
        cross.moveTo(center + QPointF(7, -7));
        cross.lineTo(center + QPointF(-7, 7));
        p.setPen(QPen(Qt::black, 5.5, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(cross);
        const QColor fill = isDown() ? QColor(210, 210, 210) : underMouse() ? QColor(240, 240, 240) : QColor(Qt::white);
        p.setPen(QPen(fill, 2.5, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(cross);
        if (hasFocus()) {
            const QRectF focusRect = QRectF(rect()).adjusted(4, 4, -4, -4);
            p.setPen(QPen(Qt::black, 3));
            p.drawRoundedRect(focusRect, 5, 5);
            p.setPen(QPen(Qt::white, 1, Qt::DotLine));
            p.drawRoundedRect(focusRect, 5, 5);
        }
    }
};
constexpr int minimumRegionWidth = 240;
constexpr int minimumRegionHeight = 72;
QPoint globalMouse(QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

struct ModeSpec {
    PixelTransform::Mode mode;
    const char *objectName;
    const char *label;
    char letter;
};
const ModeSpec modeSpecs[] = {
    {PixelTransform::Mode::Protanopia, "modeProtanopia", "Protanopia", 'P'},
    {PixelTransform::Mode::Deuteranopia, "modeDeuteranopia", "Deuteranopia", 'D'},
    {PixelTransform::Mode::Tritanopia, "modeTritanopia", "Tritanopia", 'T'}
};
constexpr int circleDiameter = 30;
constexpr int circleGap = 6;
constexpr int circleMargin = 4;
constexpr int popupWidth = circleDiameter + 2 * circleMargin;
constexpr int popupHeight = 3 * circleDiameter + 2 * circleGap + 2 * circleMargin;
}

CaptureHandle::CaptureHandle(QWidget *parent) : QPushButton(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
}
void CaptureHandle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::black, 1.5));
    p.setBrush(isDown() ? QColor(210, 210, 210) : underMouse() ? QColor(240, 240, 240) : QColor(Qt::white));
    p.drawEllipse(rect().center(), 14, 14);
    if (currentMode != PixelTransform::Mode::Original) {
        QFont onFont = QApplication::font();
        onFont.setBold(true);
        onFont.setPixelSize(10);
        const QString text = QStringLiteral("ON");
        const QRect box = QFontMetrics(onFont).tightBoundingRect(text);
        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;
        p.setFont(onFont);
        p.setPen(QPen(Qt::black));
        p.drawText(QPointF(cx - box.width() / 2.0 - box.left(), cy - box.height() / 2.0 - box.top()), text);
    }
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 1, Qt::DotLine));
        p.drawEllipse(rect().center(), 11, 11);
    }
}
ModeCircleButton::ModeCircleButton(QWidget *parent) : QPushButton(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
}
void ModeCircleButton::setCircle(int diameter, char letter, PixelTransform::Mode mode,
                                 const QString &name, const QString &tip)
{
    setFixedSize(diameter, diameter);
    this->letter = QChar::fromLatin1(letter);
    this->mode_ = mode;
    setAccessibleName(name);
    setToolTip(tip);
}
void ModeCircleButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor fill = active ? QColor(Qt::white)
        : isDown() ? QColor(90, 90, 90) : underMouse() ? QColor(70, 70, 70) : QColor(42, 42, 42);
    const QColor letterColor = active ? QColor(Qt::black) : QColor(Qt::white);
    const QColor ring = active ? QColor(Qt::black) : QColor(Qt::white);
    const qreal radius = width() / 2.0 - 1.0;
    const QPointF center = QRectF(rect()).center();
    p.setPen(QPen(ring, 2));
    p.setBrush(fill);
    p.drawEllipse(center, radius, radius);
    QFont font = QApplication::font();
    font.setBold(true);
    font.setPixelSize(qRound(width() * 0.6));
    p.setFont(font);
    p.setPen(QPen(letterColor));
    p.drawText(rect(), Qt::AlignCenter, QString(letter));
    if (hasFocus()) {
        p.setPen(QPen(Qt::white, 1, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, radius - 3.0, radius - 3.0);
    }
}
ModePopup::ModePopup(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setObjectName(QStringLiteral("simulationMenu"));
    setAttribute(Qt::WA_TranslucentBackground);
    for (int i = 0; i < 3; ++i) {
        const ModeSpec &spec = modeSpecs[i];
        auto *button = new ModeCircleButton(this);
        button->setObjectName(QLatin1String(spec.objectName));
        button->setGeometry(circleMargin, circleMargin + i * (circleDiameter + circleGap),
                            circleDiameter, circleDiameter);
        button->setCircle(circleDiameter, spec.letter, spec.mode,
                          tr(spec.label), tr(spec.label));
        connect(button, &QPushButton::clicked, this, [this, button] { chooseFrom(button); });
        circles[i] = button;
    }
    resize(popupWidth, popupHeight);
    QRegion mask;
    for (auto *button : circles)
        mask = mask.united(QRegion(button->geometry()));
    setMask(mask);
}
void ModePopup::setMode(PixelTransform::Mode mode)
{
    currentMode = mode;
    for (auto *button : circles)
        button->setActive(button->mode() == mode);
}
void ModePopup::dismissAsSelf()
{
    closingSelf = true;
    externallyClosed = false;
}
void ModePopup::closeFromToggle()
{
    dismissAsSelf();
    close();
}
bool ModePopup::consumeExternalDismissal()
{
    if (externallyClosed && externalTimer.isValid() && externalTimer.elapsed() < 400) {
        externallyClosed = false;
        return true;
    }
    externallyClosed = false;
    return false;
}
void ModePopup::hideEvent(QHideEvent *event)
{
    if (!closingSelf) {
        externallyClosed = true;
        externalTimer.restart();
    }
    closingSelf = false;
    QWidget::hideEvent(event);
}
void ModePopup::chooseFrom(ModeCircleButton *button)
{
    const PixelTransform::Mode chosen = button ? button->mode() : PixelTransform::Mode::Original;
    emit modeChosen(chosen == currentMode ? PixelTransform::Mode::Original : chosen);
    dismissAsSelf();
    close();
}
void ModePopup::moveFocus(int step)
{
    QWidget *focused = focusWidget();
    int index = -1;
    for (int i = 0; i < 3; ++i)
        if (circles[i] == focused) { index = i; break; }
    if (index < 0) { circles[0]->setFocus(); return; }
    circles[(index + step + 3) % 3]->setFocus();
}
void ModePopup::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape: dismissAsSelf(); close(); return;
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        auto *focused = qobject_cast<ModeCircleButton *>(focusWidget());
        if (focused) chooseFrom(focused);
        return;
    }
    case Qt::Key_Up:
    case Qt::Key_Left: moveFocus(-1); return;
    case Qt::Key_Down:
    case Qt::Key_Right: moveFocus(1); return;
    default: QWidget::keyPressEvent(event);
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
    handle->setAccessibleName(tr("Capture controls"));
    handle->setAccessibleDescription(tr("Click to choose a color vision simulation. Drag to move. Arrow keys move when focused."));
    handle->setToolTip(tr("Click for simulation; drag to move"));
    handle->setCursor(Qt::SizeAllCursor);
    handle->setGeometry(0, 0, 44, 44);
handle->installEventFilter(this);
    modePopup = new ModePopup(controls);
    connect(modePopup, &ModePopup::modeChosen, this, [this](PixelTransform::Mode mode) {
        setSimulationMode(mode);
    });
    connect(handle, &QPushButton::clicked, this, [this] {
        if (!dragged) {
            if (modePopup->isVisible()) { modePopup->closeFromToggle(); return; }
            if (modePopup->consumeExternalDismissal()) return;
#ifdef Q_OS_WIN
            if (QGuiApplication::platformName() == QStringLiteral("windows"))
                SetWindowDisplayAffinity(reinterpret_cast<HWND>(modePopup->winId()), 0x11);
#endif
            modePopup->setMode(simulationMode);
            QPoint position = handle->mapToGlobal(QPoint(0, handle->height() + 2));
            position.rx() += (handle->width() - modePopup->width()) / 2;
            const QScreen *screen = QGuiApplication::screenAt(position);
            const QRect area = screen ? screen->availableGeometry()
                                      : QGuiApplication::primaryScreen()->geometry();
            const int right = area.x() + area.width() - modePopup->width();
            const int bottom = area.y() + area.height() - modePopup->height();
            if (position.x() > right) position.setX(right);
            if (position.x() < area.x()) position.setX(area.x());
            if (position.y() > bottom)
                position.setY(handle->mapToGlobal(QPoint(0, -modePopup->height() - 2)).y());
            if (position.y() < area.y()) position.setY(area.y());
            modePopup->move(position);
            modePopup->show();
            modePopup->raise();
        }
        dragged = false;
    });
    closeButton = new CaptureCloseButton(controls);
    closeButton->setAccessibleName(tr("Close capture"));
    closeButton->setToolTip(tr("Close capture"));
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    hazardButton = new QPushButton(tr("Hazard Mode"), controls);
    hazardButton->setObjectName(QStringLiteral("hazardModeButton"));
    hazardButton->setCheckable(true);
    hazardButton->setFocusPolicy(Qt::StrongFocus);
    hazardButton->setAccessibleName(tr("Hazard Mode"));
    hazardButton->setToolTip(tr("Outline detected edges with contrast below 2:1"));
    hazardButton->setGeometry(48, 7, 136, 30);
    hazardButton->setStyleSheet(QStringLiteral("QPushButton { color: white; background: #252525; border: 1px solid white; border-radius: 6px; } QPushButton:checked { background: #9f2020; } QPushButton:focus { border: 2px solid #ffdf00; }"));
    connect(hazardButton, &QPushButton::toggled, this, [this](bool enabled) {
        hazardEnabled = enabled;
        hazardRegions.clear();
        if (capture) capture->setHazardEnabled(enabled);
        update();
    });
    setTabOrder(handle, hazardButton);
    setTabOrder(hazardButton, closeButton);
    resizeHandle = new QPushButton(QStringLiteral("↘"), controls);
    resizeHandle->setObjectName(QStringLiteral("captureResizeHandle"));
    resizeHandle->setAccessibleName(tr("Resize capture"));
    resizeHandle->setToolTip(tr("Drag to resize; arrow keys resize when focused"));
    resizeHandle->setAccessibleDescription(resizeHandle->toolTip());
    resizeHandle->setFocusPolicy(Qt::StrongFocus);
    resizeHandle->setCursor(Qt::SizeFDiagCursor);
    resizeHandle->setStyleSheet(QStringLiteral("QPushButton { color: white; background: #252525; border: 1px solid white; border-radius: 6px; font-size: 20px; } QPushButton:focus { border: 2px solid #ffdf00; }"));
    resizeHandle->installEventFilter(this);
    setTabOrder(closeButton, resizeHandle);
    setMinimumSize(minimumRegionWidth, minimumRegionHeight);
    setGeometry(region);
    positionControls();
}
CapturePreview::~CapturePreview()
{
    // Stop before QWidget and its native capture/control windows are destroyed.
    delete capture;
}
void CapturePreview::setSimulationMode(PixelTransform::Mode mode)
{
    simulationMode = mode;
    if (handle) handle->setMode(mode);
    if (capture) capture->setSimulationMode(mode);
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
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath outline;
    outline.addRoundedRect(outer, 12, 12);
    p.save();
    p.setClipPath(outline);
    if (!frame.isNull()) p.drawImage(rect(), frame);
    if (hazardEnabled && !frame.isNull()) {
        QPen hazardPen(Qt::red, 2);
        hazardPen.setCosmetic(true);
        p.setPen(hazardPen);
        p.setBrush(Qt::NoBrush);
        const qreal scaleX = qreal(width()) / frame.width();
        const qreal scaleY = qreal(height()) / frame.height();
        for (const auto &region : hazardRegions) {
            if (!(region.contrastRatio < EdgeDetection::ContrastAlarmThreshold)) continue;
            // Region endpoints are inclusive frame pixels; scale with the same
            // frame being displayed, even while a newer geometry is requested.
            const QRect pixels(region.startPixel, region.endPixel);
            if (!pixels.isValid()) continue;
            p.drawRect(QRectF(pixels.x() * scaleX, pixels.y() * scaleY,
                              pixels.width() * scaleX, pixels.height() * scaleY));
        }
    }
    if (!captureError.isEmpty()) {
        p.setPen(Qt::white);
        p.drawText(rect().adjusted(12, 48, -12, -12), Qt::TextWordWrap, captureError);
    }
    p.restore();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::black, 1));
    p.drawRoundedRect(outer, 12, 12);
    p.setPen(QPen(Qt::white, 1));
    p.drawRoundedRect(outer.adjusted(1, 1, -1, -1), 11, 11);
}
void CapturePreview::resizeEvent(QResizeEvent *event)
{
    positionControls();
    QWidget::resizeEvent(event);
}
void CapturePreview::positionControls()
{
    controls->setGeometry(geometry());
    closeButton->setGeometry(width()-44, 0, 44, 44);
    resizeHandle->setGeometry(width()-28, height()-28, 28, 28);
    controls->setMask(QRegion(handle->geometry()).united(QRegion(closeButton->geometry()))
                      .united(QRegion(hazardButton->geometry()))
                      .united(QRegion(resizeHandle->geometry())));
    if (capture) {
        // Retain the last image until the worker returns pixels for the new region.
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
        connect(capture, &DesktopCapture::frameReady, this, &CapturePreview::presentFrame);
        connect(capture, &DesktopCapture::captureFailed, this, [this](const QString &error) {
            captureError = error; frame = QImage();
            setAccessibleDescription(error); update();
        });
        capture->setRegion(physicalRegion());
        capture->setSimulationMode(simulationMode);
        capture->setHazardEnabled(hazardEnabled);
        capture->start();
#endif
    }
}
void CapturePreview::presentFrame(const QImage &image, const QRect &area,
                                 const std::vector<EdgeDetection::Region> &regions)
{
    // One frame is in flight at a time. Exact geometry matching starves painting
    // during movement. Show completed frames; the next capture uses the latest
    // region and converges when movement stops, without a queued frame backlog.
    if (!image.isNull() && !area.isEmpty()) {
        frame = image;
        hazardRegions = hazardEnabled ? regions : std::vector<EdgeDetection::Region>{};
        update();
    }
    if (capture) capture->acknowledgeFrame();
}
void CapturePreview::hideEvent(QHideEvent *event)
{
    controls->hide();
    if (capture) capture->requestInterruption();
    QWidget::hideEvent(event);
}
bool CapturePreview::eventFilter(QObject *object, QEvent *event)
{
    if (object == resizeHandle) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                resizing = true;
                resizePress = globalMouse(mouse);
                resizeStart = size();
                resizeHandle->setFocus(Qt::MouseFocusReason);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && resizing) {
            const QPoint delta = globalMouse(static_cast<QMouseEvent *>(event)) - resizePress;
            resize((resizeStart + QSize(delta.x(), delta.y())).expandedTo(minimumSize()));
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            resizing = false;
            return true;
        } else if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            QSize delta;
            switch (key->key()) {
            case Qt::Key_Left: delta = QSize(-10, 0); break;
            case Qt::Key_Right: delta = QSize(10, 0); break;
            case Qt::Key_Up: delta = QSize(0, -10); break;
            case Qt::Key_Down: delta = QSize(0, 10); break;
            default: return QWidget::eventFilter(object, event);
            }
            resize((size() + delta).expandedTo(minimumSize()));
            return true;
        }
    }
    if (object == handle) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                moving = true; dragged = false;
                pressPosition = globalMouse(mouse);
                dragOffset = pressPosition - pos();
            }
        } else if (event->type() == QEvent::MouseMove && moving) {
            const QPoint pointer = globalMouse(static_cast<QMouseEvent *>(event));
            if ((pointer - pressPosition).manhattanLength() >= QApplication::startDragDistance())
                dragged = true;
            if (dragged) move(pointer - dragOffset);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            moving = false;
        } else if (event->type() == QEvent::KeyPress) {
            dragged = false;
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
