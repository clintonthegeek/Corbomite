// SPDX-License-Identifier: GPL-3.0-or-later
//
// 2026-04-27 — HoverPopover Mod-key pinning + 500ms grace timer (P2 #4
// sub-items 1+4). Spec:
// docs/superpowers/specs/2026-04-27-hoverpopover-pinning-grace-design.md
//
// Verifies the explicit State enum transitions: Hidden ↔ Pending ↔ Visible
// ↔ Pinned. Pinning is latching via Ctrl-press while Visible; Esc and
// outside-click unpin. Grace timer keeps the popover alive when the source
// link hover ends but the cursor is over the popover, and dismisses
// otherwise. Replacement on a new scheduleShow always wins, even when
// pinned.

#include <QApplication>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QString>
#include <QTest>

#include "editor/HoverPopover.h"

using Corbomite::HoverPopover;

class TstHoverPopoverPinning : public QObject
{
    Q_OBJECT

private slots:
    void pinTransitionsOnCtrlPress();
    void escUnpins();
    void outsideClickUnpins();
    void graceTimerKeepsOpenIfCursorOverPopover();
    void graceTimerHidesIfCursorElsewhere();
    void replacementWinsOverPinned();
    void pinningPersistsAcrossLinkHoverEnded();
};

namespace {

// Drive the popover from Hidden → Pending → Visible deterministically.
// scheduleShow + a wait that exceeds the 300ms delay constant.
void showAndWaitVisible(HoverPopover &popover, const QString &target)
{
    popover.scheduleShow(target, QPoint(10, 10));
    QTRY_COMPARE_WITH_TIMEOUT(popover.stateForTest(),
                              HoverPopover::State::Visible, 1000);
}

// Send a Ctrl-press through the application event filter chain. The
// filter watches qApp, so QCoreApplication::sendEvent on qApp is the
// shortest path — QTest::keyEvent requires a focus widget which the
// non-focusable popover doesn't provide.
void postCtrlPressToApp()
{
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Control, Qt::NoModifier);
    QCoreApplication::sendEvent(qApp, &press);
}

void postMousePressToApp(const QPoint &globalPos)
{
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(globalPos),  // local — unused in the filter
                      QPointF(globalPos),  // scenePosition
                      QPointF(globalPos),  // globalPosition
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(qApp, &press);
}

} // namespace

void TstHoverPopoverPinning::pinTransitionsOnCtrlPress()
{
    HoverPopover popover;
    showAndWaitVisible(popover, QStringLiteral("Foo.md"));

    postCtrlPressToApp();

    QCOMPARE(popover.stateForTest(), HoverPopover::State::Pinned);
    QVERIFY(popover.isPinned());
    QVERIFY(popover.isVisible());
}

void TstHoverPopoverPinning::escUnpins()
{
    HoverPopover popover;
    showAndWaitVisible(popover, QStringLiteral("Foo.md"));
    postCtrlPressToApp();
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Pinned);

    // Esc routed through the app filter (popover doesn't accept focus,
    // so the bare keyPressEvent override doesn't fire under offscreen).
    QKeyEvent escEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(qApp, &escEvent);

    QCOMPARE(popover.stateForTest(), HoverPopover::State::Hidden);
    QVERIFY(!popover.isVisible());
}

void TstHoverPopoverPinning::outsideClickUnpins()
{
    HoverPopover popover;
    showAndWaitVisible(popover, QStringLiteral("Foo.md"));
    popover.move(QPoint(100, 100));
    postCtrlPressToApp();
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Pinned);

    // Click well outside the popover's frame (frame is at 100,100 with
    // fixed width 380 + max height 280). 5,5 is comfortably outside.
    postMousePressToApp(QPoint(5, 5));

    QCOMPARE(popover.stateForTest(), HoverPopover::State::Hidden);
}

void TstHoverPopoverPinning::graceTimerKeepsOpenIfCursorOverPopover()
{
    HoverPopover popover;
    popover.move(QPoint(100, 100));
    showAndWaitVisible(popover, QStringLiteral("Foo.md"));

    // Position the global cursor inside the popover's frame so the grace
    // timeout's widgetAt check finds us. Under offscreen the QCursor::setPos
    // call is honoured for QCursor::pos() reads even though no real cursor
    // is visible.
    const QPoint inside = popover.frameGeometry().center();
    QCursor::setPos(inside);

    popover.linkHoverEnded();
    // Wait > 500ms grace + slack. State must remain Visible (the timeout
    // saw the cursor inside us).
    QTest::qWait(700);
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Visible);
    QVERIFY(popover.isVisible());
}

void TstHoverPopoverPinning::graceTimerHidesIfCursorElsewhere()
{
    HoverPopover popover;
    popover.move(QPoint(500, 500));
    showAndWaitVisible(popover, QStringLiteral("Foo.md"));

    // Park the cursor far from the popover's frame.
    QCursor::setPos(QPoint(5, 5));

    popover.linkHoverEnded();
    // Stays Visible during the grace window, then hides.
    QTest::qWait(700);
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Hidden);
    QVERIFY(!popover.isVisible());
}

void TstHoverPopoverPinning::replacementWinsOverPinned()
{
    HoverPopover popover;
    showAndWaitVisible(popover, QStringLiteral("Foo.md"));
    postCtrlPressToApp();
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Pinned);

    // A new hover for a different target replaces the pinned popover —
    // it transitions through Hidden → Pending and lands back at Visible
    // with the new target after the 300ms delay.
    popover.scheduleShow(QStringLiteral("Bar.md"), QPoint(20, 20));
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Pending);
    QVERIFY(!popover.isPinned());

    QTRY_COMPARE_WITH_TIMEOUT(popover.stateForTest(),
                              HoverPopover::State::Visible, 1000);
}

void TstHoverPopoverPinning::pinningPersistsAcrossLinkHoverEnded()
{
    HoverPopover popover;
    showAndWaitVisible(popover, QStringLiteral("Foo.md"));
    postCtrlPressToApp();
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Pinned);

    // Pinned state must ignore link-hover-end — the user moved off the
    // source link but pinned the popover deliberately.
    popover.linkHoverEnded();
    QTest::qWait(700);  // exceed grace window
    QCOMPARE(popover.stateForTest(), HoverPopover::State::Pinned);
    QVERIFY(popover.isVisible());
}

QTEST_MAIN(TstHoverPopoverPinning)
#include "tst_hover_popover_pinning.moc"
