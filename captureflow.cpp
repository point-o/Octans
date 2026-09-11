#include "captureflow.h"
#include "desktopcapture.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QFontMetrics>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QMenu>
#include <QActionGroup>
#include <QAction>
#include <QPushButton>
#include <QRegion>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QAccessible>
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
    const char *subtitle;
};
const ModeSpec modeSpecs[] = {
    {PixelTransform::Mode::Original, "modeOriginal", "Original", 'O', "Unaltered colors"},
    {PixelTransform::Mode::Protanopia, "modeProtanopia", "Protanopia", 'P', "Red-cone simulation"},
    {PixelTransform::Mode::Deuteranopia, "modeDeuteranopia", "Deuteranopia", 'D', "Green-cone simulation"},
    {PixelTransform::Mode::Tritanopia, "modeTritanopia", "Tritanopia", 'T', "Blue-cone simulation"}
};
bool highContrast() {
#ifdef Q_OS_WIN
    HIGHCONTRAST info{};
    info.cbSize = sizeof(info);
    return SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(info), &info, 0)
        && (info.dwFlags & HCF_HIGHCONTRASTON);
#else
    return false;
#endif
}
struct LensColors { QColor base, hover, text, secondary, accent; };
LensColors lensColors(const QWidget *widget) {
    const QPalette pal = widget->palette();
    if (highContrast()) return {pal.color(QPalette::Window), pal.color(QPalette::Highlight),
        pal.color(QPalette::WindowText), pal.color(QPalette::WindowText), pal.color(QPalette::WindowText)};
    return {QColor("#171c24"), QColor("#303846"), QColor("#f7f8fa"), QColor("#bfc7d3"), QColor("#ffdf00")};
}
}

CaptureHandle::CaptureHandle(QWidget *parent) : QPushButton(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setMode(PixelTransform::Mode::Original);
}
void CaptureHandle::setMode(PixelTransform::Mode mode)
{
    if (static_cast<int>(mode) < 0 || static_cast<int>(mode) >= 4)
        mode = PixelTransform::Mode::Original;
    currentMode = mode;
    const auto &spec = modeSpecs[static_cast<int>(mode)];
    setAccessibleName(tr("Simulation: %1. Choose mode or drag to move.").arg(tr(spec.label)));
    setToolTip(tr("%1 · Click to choose simulation; drag to move").arg(tr(spec.label)));
    update();
}
void CaptureHandle::paintEvent(QPaintEvent *)
{
    const auto colors = lensColors(this);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width()/2.0, height()/2.0);
    const bool on = currentMode != PixelTransform::Mode::Original;
    const bool highlighted = underMouse() || isDown();
    const QColor foreground = highContrast() && highlighted ? palette().color(QPalette::HighlightedText) : colors.text;
    const QColor ring = highContrast() ? foreground : (on ? colors.accent : colors.secondary);
    p.setPen(QPen(ring, on ? 2.5 : 1.5));
    p.setBrush(underMouse() || isDown() ? colors.hover : colors.base);
    p.drawEllipse(center, 17, 17);
    p.setPen(QPen(foreground, 1.5));
    p.setBrush(Qt::NoBrush);
    if (on) {
        QFont labelFont = font(); labelFont.setBold(true); labelFont.setPixelSize(18);
        p.setFont(labelFont);
        p.drawText(rect(), Qt::AlignCenter, QString(QChar::fromLatin1(modeSpecs[static_cast<int>(currentMode)].letter)));
    } else {
        p.drawEllipse(center, 7, 7);
        p.drawLine(center+QPointF(-11,0),center+QPointF(-5,0));
        p.drawLine(center+QPointF(5,0),center+QPointF(11,0));
        p.drawLine(center+QPointF(0,-11),center+QPointF(0,-5));
        p.drawLine(center+QPointF(0,5),center+QPointF(0,11));
    }
    if (hasFocus()) {
        p.setPen(QPen(Qt::black, 2));
        p.drawEllipse(center, 20, 20);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(center, 18, 18);
    }
}
ModeCircleButton::ModeCircleButton(QWidget *parent) : QPushButton(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setCheckable(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}
void ModeCircleButton::setCircle(int, char letter, PixelTransform::Mode mode,
                                 const QString &name, const QString &tip)
{
    this->letter = QChar::fromLatin1(letter);
    mode_ = mode;
    subtitle = tip;
    setText(name);
    setAccessibleName(name);
    setAccessibleDescription(tip);
    setToolTip(tip);
    updateGeometry();
}
QSize ModeCircleButton::sizeHint() const
{
    QFont titleFont = font(); titleFont.setBold(true);
    const QFontMetrics titleMetrics(titleFont), metrics(font());
    const int line = metrics.height();
    return QSize(qMax(titleMetrics.horizontalAdvance(text()), metrics.horizontalAdvance(subtitle)) + line*3 + 60,
                 qMax(44, titleMetrics.height()+line+24));
}
void ModeCircleButton::paintEvent(QPaintEvent *)
{
    const auto colors = lensColors(this);
    const bool highlighted = isChecked() || underMouse() || isDown();
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(highlighted ? colors.hover : colors.base);
    p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1), 8, 8);
    const QColor foreground = highContrast() && highlighted ? palette().color(QPalette::HighlightedText) : colors.text;
    const int line = fontMetrics().height();
    const int badge = qMax(28, line+8);
    const QRect badgeRect(12, (height()-badge)/2, badge, badge);
    p.setPen(QPen(isChecked() ? (highContrast() ? foreground : colors.accent) : colors.secondary, isChecked() ? 2 : 1));
    p.setBrush(Qt::NoBrush); p.drawEllipse(badgeRect);
    QFont titleFont = font(); titleFont.setBold(true); p.setFont(titleFont); p.setPen(foreground);
    p.drawText(badgeRect, Qt::AlignCenter, QString(letter));
    const int textX = badgeRect.right()+13;
    const int textY = (height()-QFontMetrics(titleFont).height()-line)/2;
    p.drawText(QRect(textX,textY,width()-textX-line-24,QFontMetrics(titleFont).height()),Qt::AlignLeft|Qt::AlignVCenter,text());
    p.setFont(font()); p.setPen(highContrast() ? foreground : colors.secondary);
    p.drawText(QRect(textX,textY+QFontMetrics(titleFont).height(),width()-textX-line-24,line),Qt::AlignLeft|Qt::AlignVCenter,subtitle);
    if (isChecked()) {
        p.setPen(QPen(highContrast() ? foreground : colors.accent,2,Qt::SolidLine,Qt::RoundCap));
        const QPointF c(width()-18,height()/2.0);
        p.drawLine(c+QPointF(-5,0),c+QPointF(-1,4)); p.drawLine(c+QPointF(-1,4),c+QPointF(6,-4));
    }
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush); p.setPen(QPen(foreground,2));
        p.drawRoundedRect(QRectF(rect()).adjusted(3,3,-3,-3),6,6);
    }
}
ModePopup::ModePopup(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setObjectName(QStringLiteral("simulationMenu"));
    setAccessibleName(tr("Color vision simulation"));
    setAttribute(Qt::WA_TranslucentBackground);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8); layout->setSpacing(4);
    for (int i = 0; i < 4; ++i) {
        const auto &spec = modeSpecs[i];
        auto *button = new ModeCircleButton(this);
        button->setObjectName(QLatin1String(spec.objectName));
        button->setCircle(44,spec.letter,spec.mode,tr(spec.label),tr(spec.subtitle));
        button->installEventFilter(this);
        connect(button,&QPushButton::clicked,this,[this,button] { chooseFrom(button); });
        layout->addWidget(button); circles[i] = button;
        if(i) setTabOrder(circles[i-1],button);
    }
    layout->setSizeConstraint(QLayout::SetFixedSize);
    setMode(PixelTransform::Mode::Original);
}
void ModePopup::paintEvent(QPaintEvent *)
{
    const auto colors = lensColors(this);
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(colors.base); p.setPen(QPen(colors.secondary,1));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5,0.5,-0.5,-0.5),12,12);
}
void ModePopup::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    for (auto *button : circles) if(button->mode()==currentMode) button->setFocus(Qt::PopupFocusReason);
}
void ModePopup::setMode(PixelTransform::Mode mode)
{
    if (static_cast<int>(mode) < 0 || static_cast<int>(mode) >= 4)
        mode = PixelTransform::Mode::Original;
    currentMode = mode;
    for (auto *button : circles) button->setActive(button->mode()==mode);
    layout()->activate(); adjustSize();
}
void ModePopup::dismissAsSelf() { closingSelf = true; externallyClosed = false; }
void ModePopup::closeFromToggle() { dismissAsSelf(); close(); }
bool ModePopup::consumeExternalDismissal()
{
    if (externallyClosed && externalTimer.isValid() && externalTimer.elapsed()<400) {
        externallyClosed=false; return true;
    }
    externallyClosed=false; return false;
}
void ModePopup::hideEvent(QHideEvent *event)
{
    if (!closingSelf) { externallyClosed=true; externalTimer.restart(); }
    else if (parentWidget()) {
        if(auto *handle=parentWidget()->findChild<CaptureHandle *>(QStringLiteral("captureHandle")))
            QTimer::singleShot(0, handle, [handle] {
                handle->window()->activateWindow();
                handle->setFocus(Qt::PopupFocusReason);
            });
    }
    closingSelf=false;
    QWidget::hideEvent(event);
}
void ModePopup::chooseFrom(ModeCircleButton *button)
{
    const auto chosen=button ? button->mode() : PixelTransform::Mode::Original;
    const auto resolved = chosen==currentMode ? PixelTransform::Mode::Original : chosen;
    setMode(resolved);
    emit modeChosen(resolved);
    dismissAsSelf(); close();
}
void ModePopup::moveFocus(int step)
{
    int index=-1;
    for(int i=0;i<4;++i) if(circles[i]==focusWidget()) { index=i; break; }
    circles[index<0 ? 0 : (index+step+4)%4]->setFocus(Qt::TabFocusReason);
}
bool ModePopup::eventFilter(QObject *, QEvent *event)
{
    if(event->type()==QEvent::KeyPress) {
        auto *key=static_cast<QKeyEvent *>(event);
        switch(key->key()) {
        case Qt::Key_Up: case Qt::Key_Down: case Qt::Key_Left: case Qt::Key_Right:
        case Qt::Key_Home: case Qt::Key_End: case Qt::Key_Return: case Qt::Key_Enter:
        case Qt::Key_Space: case Qt::Key_Escape: case Qt::Key_Tab: case Qt::Key_Backtab:
            keyPressEvent(key); return true;
        }
    }
    return false;
}
void ModePopup::keyPressEvent(QKeyEvent *event)
{
    switch(event->key()) {
    case Qt::Key_Escape: dismissAsSelf(); close(); return;
    case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space:
        if(auto *button=qobject_cast<ModeCircleButton *>(focusWidget())) button->click();
        return;
    case Qt::Key_Home: circles[0]->setFocus(); return;
    case Qt::Key_End: circles[3]->setFocus(); return;
    case Qt::Key_Up: case Qt::Key_Left: case Qt::Key_Backtab: moveFocus(-1); return;
    case Qt::Key_Down: case Qt::Key_Right: moveFocus(1); return;
    case Qt::Key_Tab: moveFocus(event->modifiers()&Qt::ShiftModifier ? -1 : 1); return;
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
    handle->setMode(PixelTransform::Mode::Original);
    handle->setAccessibleDescription(tr("Click to choose a color vision simulation. Drag to move. Arrow keys move when focused."));

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
    hazardMenu = new QMenu(tr("Hazard threshold"), controls);
    hazardMenu->setObjectName(QStringLiteral("hazardThresholdMenu"));
    auto *group = new QActionGroup(hazardMenu);
    group->setExclusive(true);
    const struct { const char *label; const char *tip; float value; } presets[] = {
        {"2:1", "Default alarm: outline below 2:1", EdgeDetection::DefaultAlarmThreshold},
        {"3:1", "WCAG graphics and large-text level", 3.0f},
        {"4.5:1", "WCAG normal-text level", 4.5f}
    };
    for (const auto &preset : presets) {
        QAction *action = hazardMenu->addAction(tr(preset.label));
        action->setCheckable(true);
        action->setChecked(preset.value == hazardThreshold);
        action->setToolTip(tr(preset.tip));
        action->setData(double(preset.value));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, action, preset] {
            if (!action->isChecked()) return;
            hazardThreshold = preset.value;
            update();
        });
    }
    // Clicking toggles Hazard Mode; right-click picks the alarm threshold.
    hazardButton->setToolTip(tr("Outline detected edges with contrast below the alarm level. Right-click, Menu key, or Shift+F10 to choose the level."));
    hazardButton->setAccessibleDescription(hazardButton->toolTip());
    hazardButton->installEventFilter(this);
    hazardButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hazardButton, &QPushButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        hazardMenu->popup(hazardButton->mapToGlobal(pos));
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
        p.setBrush(Qt::NoBrush);
        const qreal scaleX = qreal(width()) / frame.width();
        const qreal scaleY = qreal(height()) / frame.height();
        struct Mark { QRectF rect; EdgeDetection::Severity severity; };
        QVector<Mark> marks;
        for (const auto &region : hazardRegions) {
            const EdgeDetection::Severity severity = EdgeDetection::severityFor(
                region.contrastRatio, region.indeterminate, hazardThreshold);
            if (severity == EdgeDetection::Severity::Hidden) continue;
            // Region endpoints are inclusive frame pixels; scale with the same
            // frame being displayed, even while a newer geometry is requested.
            const QRect pixels(region.startPixel, region.endPixel);
            if (!pixels.isValid()) continue;
            marks.append({QRectF(pixels.x() * scaleX, pixels.y() * scaleY,
                                 pixels.width() * scaleX, pixels.height() * scaleY),
                          severity});
        }
        // Dual-tone strokes: the black underlay carries the geometry on any
        // background and for any color-vision deficiency; the colored core
        // carries severity for those with intact red-green and blue-yellow
        // vision. Dashes mark boundaries the ratio cannot vouch for.
        for (const Mark &mark : marks) {
            const QRectF rect = mark.rect;
            p.setBrush(Qt::NoBrush);
            QPen core(Qt::red, 2, Qt::SolidLine);
            QPen underlay(Qt::black, 4, Qt::SolidLine);
            if (mark.severity == EdgeDetection::Severity::Critical) {
                underlay.setWidthF(6);
            } else if (mark.severity == EdgeDetection::Severity::Marginal) {
                core = QPen(QColor(255, 165, 0), 2, Qt::SolidLine);
            } else if (mark.severity == EdgeDetection::Severity::Indeterminate) {
                underlay = QPen(Qt::black, 4, Qt::DashLine);
                core = QPen(Qt::white, 2, Qt::DashLine);
            }
            underlay.setCosmetic(true);
            core.setCosmetic(true);
            p.setPen(underlay);
            p.drawRect(rect);
            p.setPen(core);
            p.drawRect(rect);
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
    if (!capture) {
        controls->activateWindow();
        handle->setFocus(Qt::OtherFocusReason);
    }
    if (!capture && QGuiApplication::platformName() == QStringLiteral("windows")) {
#ifdef Q_OS_WIN
        // Exclude both top-level windows before starting, or never capture at all.
        constexpr DWORD excludeFromCapture = 0x11;
        if (!SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), excludeFromCapture)
            || !SetWindowDisplayAffinity(reinterpret_cast<HWND>(controls->winId()), excludeFromCapture)) {
            captureError = tr("Windows could not exclude Octans from capture. Capture stopped to prevent feedback.");
            setAccessibleDescription(captureError);
            handle->setAccessibleDescription(captureError);
            controls->setAccessibleDescription(captureError);
            QAccessibleEvent accessibleError(handle, QAccessible::DescriptionChanged);
            QAccessible::updateAccessibility(&accessibleError);
            update();
            return;
        }
        capture = new DesktopCapture(this);
        connect(capture, &DesktopCapture::frameReady, this, &CapturePreview::presentFrame);
        connect(capture, &DesktopCapture::captureFailed, this, [this](const QString &error) {
            captureError = error; frame = QImage();
            setAccessibleDescription(error); update();
            handle->setAccessibleDescription(error);
            controls->setAccessibleDescription(error);
            QAccessibleEvent accessibleError(handle, QAccessible::DescriptionChanged);
            QAccessible::updateAccessibility(&accessibleError);
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
    if (object == hazardButton && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Menu || (key->key() == Qt::Key_F10 && (key->modifiers() & Qt::ShiftModifier))) {
            hazardMenu->popup(hazardButton->mapToGlobal(QPoint(0,hazardButton->height())));
            return true;
        }
    }
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
