#include "captureflow.h"
#include <QAccessible>
#include <QApplication>
#include <QFontDatabase>
#include <QSignalSpy>
#include <QTest>

class PickerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        qRegisterMetaType<PixelTransform::Mode>();
        QFontDatabase::addApplicationFont(":/fonts/SpaceGrotesk.ttf");
        QApplication::setFont(QFont("Space Grotesk", 12));
    }
    void targetsAndAccessibleSelection() {
        ModePopup popup;
        popup.setMode(PixelTransform::Mode::Deuteranopia);
        popup.show();
        QTest::qWait(20);
        const auto rows = popup.findChildren<ModeCircleButton *>();
        QCOMPARE(rows.size(), 4);
        for (auto *row : rows) {
            QVERIFY(row->width() >= 44);
            QVERIFY(row->height() >= 44);
            QVERIFY(popup.rect().contains(row->geometry()));
            QVERIFY(!row->accessibleName().isEmpty());
            QVERIFY(row->isCheckable());
            auto *accessible = QAccessible::queryAccessibleInterface(row);
            QVERIFY(accessible);
            QVERIFY(accessible->state().checkable);
            QCOMPARE(bool(accessible->state().checked), row->mode() == PixelTransform::Mode::Deuteranopia);
        }
        QVERIFY(popup.grab().save("picker-selected.png"));
        CaptureHandle handle;
        handle.resize(44,44);
        handle.setMode(PixelTransform::Mode::Tritanopia);
        QVERIFY(handle.accessibleName().contains("Tritanopia"));
        QVERIFY(handle.grab().save("handle-selected.png"));
    }
    void keyboardSelectionAndDismissal() {
        QWidget host;
        CaptureHandle handle(&host);
        handle.setObjectName("captureHandle");
        handle.resize(44,44);
        host.resize(100,100);
        host.show();
        ModePopup popup(&host);
        QSignalSpy chosen(&popup, &ModePopup::modeChosen);
        popup.setMode(PixelTransform::Mode::Original);
        popup.show();
        QTest::qWait(20);
        auto *original = popup.findChild<ModeCircleButton *>("modeOriginal");
        auto *tritan = popup.findChild<ModeCircleButton *>("modeTritanopia");
        QVERIFY(original && tritan);
        QCOMPARE(popup.focusWidget(), original);
        QTest::keyClick(popup.focusWidget(), Qt::Key_End);
        QCOMPARE(popup.focusWidget(), tritan);
        QTest::keyClick(popup.focusWidget(), Qt::Key_Space);
        QCOMPARE(chosen.size(), 1);
        QCOMPARE(qvariant_cast<PixelTransform::Mode>(chosen.last().at(0)), PixelTransform::Mode::Tritanopia);
        QVERIFY(!popup.isVisible());
        popup.setMode(PixelTransform::Mode::Tritanopia);
        popup.show();
        QTest::qWait(20);
        QTest::keyClick(popup.focusWidget(), Qt::Key_Home);
        QCOMPARE(popup.focusWidget(), original);
        QTest::keyClick(popup.focusWidget(), Qt::Key_Tab);
        QCOMPARE(popup.focusWidget(), popup.findChild<ModeCircleButton *>("modeProtanopia"));
        QTest::keyClick(popup.focusWidget(), Qt::Key_Escape);
        QVERIFY(!popup.isVisible());
        QCOMPARE(chosen.size(), 1); // Dismissal doesn't change the mode.
        QTRY_COMPARE_WITH_TIMEOUT(host.focusWidget(), &handle, 1000);
        // The offscreen platform does not implement native window activation.
        if (QApplication::platformName() == QStringLiteral("windows"))
            QTRY_VERIFY_WITH_TIMEOUT(handle.hasFocus(), 1000);
    }
    void largeFontLayout() {
        const QFont saved = QApplication::font();
        QFont large = saved;
        large.setPointSize(24);
        QApplication::setFont(large);
        ModePopup popup;
        popup.show();
        QTest::qWait(20);
        const auto rows = popup.findChildren<ModeCircleButton *>();
        for (auto *row : rows) {
            QVERIFY(popup.rect().contains(row->geometry()));
            for (auto *other : rows)
                if (other != row) QVERIFY(!row->geometry().intersects(other->geometry()));
        }
        QVERIFY(popup.grab().save("picker-large-font.png"));
        QApplication::setFont(saved);
    }
};
QTEST_MAIN(PickerTest)
#include "picker_test.moc"
