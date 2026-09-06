#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QHBoxLayout>
#include <algorithm>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
// Decorative, unfocusable vector artwork; coordinates from the approved study.
class Constellation final : public QWidget
{
public:
    explicit Constellation(QWidget *parent) : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    void setInk(const QColor &color) { ink = color; update(); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal fontScale = QFontMetrics(font()).height() / 19.0;
        const qreal scale = std::min({width() * .85 / 280.0,
                                     height() / 250.0, 235.0 * fontScale / 280.0});
        p.translate((width() - 280 * scale) / 2, (height() - 250 * scale) / 2);
        p.scale(scale, scale);
        QPainterPath lines;
        lines.moveTo(34, 54);
        lines.lineTo(111, 85);
        lines.lineTo(144, 18);
        lines.lineTo(247, 226);
        lines.lineTo(111, 85);
        p.setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(lines);
        const auto star = [this, &p](qreal x, qreal y, qreal r, qreal inner) {
            QPainterPath path;
            path.moveTo(x, y-r);
            path.lineTo(x+inner, y-inner); path.lineTo(x+r, y);
            path.lineTo(x+inner, y+inner); path.lineTo(x, y+r);
            path.lineTo(x-inner, y+inner); path.lineTo(x-r, y);
            path.lineTo(x-inner, y-inner); path.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(ink);
            p.drawPath(path);
        };

        star(34,54,9,2.5); star(111,85,9,2.5); star(144,18,11,3);
        star(247,226,9,2.5); star(139,188,6,1.8);
    }
private:
    QColor ink = Qt::black;
};

bool highContrastEnabled()
{
#ifdef Q_OS_WIN
    HIGHCONTRAST contrast{};
    contrast.cbSize = sizeof(contrast);
    return SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON);
#else
    return false;
#endif
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
#ifdef Q_OS_WIN
    setWindowFlag(Qt::FramelessWindowHint);
#endif
    auto *header = new QWidget(ui->centralwidget);
    header->setObjectName(QStringLiteral("titleArea"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    ui->launchLayout->removeWidget(ui->wordmark);
    headerLayout->addWidget(ui->wordmark);
    headerLayout->addStretch();
    auto *closeButton = new QPushButton(QStringLiteral("×"), header);
    closeButton->setObjectName(QStringLiteral("closeWindowButton"));
    closeButton->setAccessibleName(tr("Close Octans"));
    closeButton->setToolTip(tr("Close Octans (Alt+F4)"));
    closeButton->setMinimumSize(44, 44);
    headerLayout->addWidget(closeButton);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    ui->launchLayout->insertWidget(0, header);
    setTabOrder(ui->newCaptureButton, closeButton);
    auto *sky = new Constellation(ui->centralwidget);
    sky->setObjectName(QStringLiteral("constellation"));
    ui->launchLayout->insertWidget(1, sky, 1);
    connect(ui->newCaptureButton, &QPushButton::clicked, this, &MainWindow::showCaptureUnavailable);
    updateAppearance();
    resize(size().expandedTo(minimumSizeHint()));
#ifdef Q_OS_WIN
    // Retain native sizing, snapping and the system menu without native title chrome.
    const HWND handle = reinterpret_cast<HWND>(winId());
    SetWindowLongPtr(handle, GWL_STYLE, GetWindowLongPtr(handle, GWL_STYLE)
                     | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::showCaptureUnavailable()
{
    QMessageBox::information(this, tr("Capture"), tr("Capture selection is not implemented yet."));
}

void MainWindow::updateAppearance()
{
    QPalette colors = QApplication::palette();
    if (!highContrastEnabled()) {
        colors.setColor(QPalette::Window, QColor("#ffdc00"));
        colors.setColor(QPalette::WindowText, Qt::black);
        colors.setColor(QPalette::Button, QColor("#ffdc00"));
        colors.setColor(QPalette::ButtonText, Qt::black);
    }
    ui->centralwidget->setPalette(colors);
    ui->centralwidget->setAutoFillBackground(true);
    const qreal scale = QFontMetrics(QApplication::font()).height() / 19.0;
    const auto px = [scale](int value) { return qRound(value * scale); };
    QFont title = QApplication::font();
    title.setFamily(QStringLiteral("Bahnschrift"));
    title.setStyleHint(QFont::SansSerif);
    title.setStretch(QFont::SemiExpanded);
    if (title.pointSizeF() > 0) title.setPointSizeF(title.pointSizeF() * 2.71875);
    else title.setPixelSize(qRound(title.pixelSize() * 2.71875));
    title.setBold(true);
    ui->wordmark->setFont(title);
    auto *closeButton = ui->centralwidget->findChild<QPushButton *>(QStringLiteral("closeWindowButton"));
    QFont closeFont = QApplication::font();
    closeFont.setFamily(QStringLiteral("Arial Black"));
    closeFont.setStyleHint(QFont::SansSerif);
    closeFont.setWeight(QFont::Black);
    if (closeFont.pointSizeF() > 0) closeFont.setPointSizeF(closeFont.pointSizeF() * 3);
    else closeFont.setPixelSize(closeFont.pixelSize() * 3);
    closeButton->setFont(closeFont);
    const QFontMetrics closeMetrics(closeFont);
    const int closeExtent = std::max(44, closeMetrics.height() + 4);
    closeButton->setMinimumSize(closeExtent, closeExtent);
    closeButton->setCursor(Qt::PointingHandCursor);
    ui->launchLayout->setContentsMargins(px(26), px(25), px(26), px(26));
    if (closeButton->parentWidget() && closeButton->parentWidget()->layout()) {
        closeButton->parentWidget()->layout()->setContentsMargins(0, 0, 0, 0);
    }
    ui->launchLayout->setSpacing(px(14));
    auto *sky = ui->centralwidget->findChild<QWidget *>(QStringLiteral("constellation"));
    sky->setMinimumHeight(px(150));
    ui->newCaptureButton->setCursor(Qt::PointingHandCursor);
    const int captureHeight = std::max(48, px(56));
    ui->newCaptureButton->setMinimumHeight(captureHeight);
    QFont captureFont = QApplication::font();
    captureFont.setFamily(QStringLiteral("Bahnschrift"));
    captureFont.setStyleHint(QFont::SansSerif);
    captureFont.setStretch(QFont::SemiExpanded);
    captureFont.setWeight(QFont::Bold);
    captureFont.setStyle(QFont::StyleNormal);
    captureFont.setPixelSize(captureHeight);
    const int glyphHeight = QFontMetrics(captureFont).tightBoundingRect(tr("Capture")).height();
    captureFont.setPixelSize(qRound(captureHeight * captureHeight * 0.4 / std::max(1, glyphHeight)));
    ui->newCaptureButton->setFont(captureFont);
    const QColor captureBackground = highContrastEnabled()
        ? colors.color(QPalette::Button) : QColor("#ffdc00");
    // Explicit colors avoid stylesheet widgets inheriting the application's dark palette.
    // The selected colors still follow Windows high-contrast mode.
    ui->centralwidget->setStyleSheet(QStringLiteral(
        "QLabel#wordmark { color: %3; }"
        "QPushButton { color: %1; background: %2;"
        " border: 2px solid transparent; padding: 8px 12px; text-align: left; }"
        "QPushButton#newCaptureButton { background: %4; border: 2px solid %1; border-radius: 0; padding: 0 12px; text-align: center; }"
        "QPushButton#closeWindowButton { text-align: center; padding: 0; }"
        "QPushButton:hover, QPushButton:pressed { background: %1; color: %2; border-radius: 10px; }"
        "QPushButton#newCaptureButton:hover, QPushButton#newCaptureButton:pressed { background: %1; color: %2; }"
        "QPushButton:focus { text-decoration: underline; }"
        "QPushButton#newCaptureButton:focus { text-decoration: none; }")
        .arg(colors.color(QPalette::ButtonText).name(), colors.color(QPalette::Button).name(),
             colors.color(QPalette::WindowText).name(), captureBackground.name()));
    ui->newCaptureButton->setPalette(colors);
    static_cast<Constellation *>(sky)->setInk(colors.color(QPalette::WindowText));
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ApplicationFontChange || event->type() == QEvent::FontChange)
        updateAppearance();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool MainWindow::nativeEvent(const QByteArray &type, void *message, qintptr *result)
#else
bool MainWindow::nativeEvent(const QByteArray &type, void *message, long *result)
#endif
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCCALCSIZE && msg->wParam) {
        // Maximized borderless windows must respect the taskbar work area.
        if (IsZoomed(msg->hwnd)) {
            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfo(MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam)->rgrc[0] = monitor.rcWork;
        }
        *result = 0;
        return true;
    }
    if (msg->message == WM_NCHITTEST) {
        POINT point{static_cast<short>(LOWORD(msg->lParam)),
                    static_cast<short>(HIWORD(msg->lParam))};
        ScreenToClient(msg->hwnd, &point);
        RECT client{};
        GetClientRect(msg->hwnd, &client);
        const int border = qRound(7 * devicePixelRatioF());
        const bool left = point.x < border, right = point.x >= client.right - border;
        const bool top = point.y < border, bottom = point.y >= client.bottom - border;
        if (!IsZoomed(msg->hwnd) && (left || right || top || bottom)) {
            *result = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP)
                      : bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                      : left ? HTLEFT : HTRIGHT;
            return true;
        }
        const QPoint local(qRound(point.x / devicePixelRatioF()),
                           qRound(point.y / devicePixelRatioF()));
        auto *header = ui->centralwidget->findChild<QWidget *>(QStringLiteral("titleArea"));
        if (!header)
            return QMainWindow::nativeEvent(type, message, result);
        auto *closeButton = header->findChild<QPushButton *>(QStringLiteral("closeWindowButton"));
        const QRect closeRect(closeButton->mapTo(this, QPoint()), closeButton->size());
        const int titleBottom = header->mapTo(this, QPoint(0, header->height())).y();
        *result = local.y() < titleBottom && !closeRect.contains(local) ? HTCAPTION : HTCLIENT;
        return true;
    }
    if (msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED)
        QTimer::singleShot(0, this, &MainWindow::updateAppearance);
#endif
    return QMainWindow::nativeEvent(type, message, result);
}
