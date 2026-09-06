#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    void changeEvent(QEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray &type, void *message, long *result) override;
#endif
private:
    void updateAppearance();
    void showCaptureUnavailable();
    Ui::MainWindow *ui;
};
#endif
