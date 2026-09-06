#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
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
        const QColor ink = palette().color(QPalette::WindowText);
        QPainterPath lines;
        lines.moveTo(34, 54);
        lines.lineTo(111, 85);
        lines.lineTo(144, 18);
        lines.lineTo(247, 226);
        lines.lineTo(111, 85);
        p.setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(lines);
        const auto star = [&p, &ink](qreal x, qreal y, qreal r, qreal inner) {
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
    auto *sky = new Constellation(ui->centralwidget);
    sky->setObjectName(QStringLiteral("constellation"));
    ui->launchLayout->insertWidget(1, sky, 1);
    ui->presetPanel->hide();
    connect(ui->presetToggle, &QPushButton::toggled, this, [this](bool open) {
        ui->presetPanel->setVisible(open);
        ui->presetToggle->setText(open ? tr("Capture with preset  −") : tr("Capture with preset  +"));
    });
    for (auto *button : {ui->newCaptureButton, ui->invertButton, ui->grayscaleButton})
        connect(button, &QPushButton::clicked, this, &MainWindow::showCaptureUnavailable);
    updateAppearance();
    resize(size().expandedTo(minimumSizeHint()));
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
    if (title.pointSizeF() > 0) title.setPointSizeF(title.pointSizeF() * 1.8125);
    else title.setPixelSize(qRound(title.pixelSize() * 1.8125));
    title.setItalic(true);
    ui->wordmark->setFont(title);
    ui->launchLayout->setContentsMargins(px(26),px(25),px(26),px(22));
    ui->launchLayout->setSpacing(px(14));
    auto *sky = ui->centralwidget->findChild<QWidget *>(QStringLiteral("constellation"));
    sky->setMinimumHeight(px(150));
    for (auto *button : {ui->newCaptureButton, ui->presetToggle, ui->invertButton, ui->grayscaleButton}) {
        button->setMinimumHeight(std::max(44, px(44)));
        button->setCursor(Qt::PointingHandCursor);
    }
    ui->newCaptureButton->setMinimumHeight(std::max(48, px(50)));
    // Palette roles preserve system contrast colors; focus does not depend on hover.
    ui->centralwidget->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; background: %2;"
        " border: 2px solid transparent; padding: 8px 12px; text-align: left; }"
        "QPushButton#newCaptureButton { border: 2px solid %1; }"
        "QPushButton:hover, QPushButton:pressed { background: %1; color: %2; }"
        "QPushButton:focus, QPushButton#newCaptureButton:focus { border: 2px dashed %1; }")
        .arg(colors.color(QPalette::ButtonText).name(), colors.color(QPalette::Button).name()));
    for (auto *button : {ui->newCaptureButton, ui->presetToggle, ui->invertButton, ui->grayscaleButton})
        button->setPalette(colors);
    sky->update();
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
    if (msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED)
        QTimer::singleShot(0, this, &MainWindow::updateAppearance);
#endif
    return QMainWindow::nativeEvent(type, message, result);
}
