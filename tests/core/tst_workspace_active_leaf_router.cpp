// tests/core/tst_workspace_active_leaf_router.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 6: layoutReady gate + WorkspaceActiveLeafRouter behaviours.
// Phase 6.2 lights up the gate-related slots (this file). Phase 6.1 will add
// focus-driven activation slots once the WorkspaceActiveLeafRouter class lands.

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

using namespace Corbomite;

class TestWorkspaceActiveLeafRouter : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void layoutReadyDefaultsTrueAfterConstruction();
    void sameLeafSetTwiceDoesNotRefire();
    void layoutNotReadySuppressesActiveLeafChange();
    void layoutBecomesReadyAfterDeferredSet();
    void setLayoutReadyFalseToTrueEmitsLayoutReadyOnce();
    void setLayoutReadySameValueIsNoOp();
    void focusInsideLeafWidgetMarksLeafActive();
    void focusOutsideMainWindowDoesNotChangeActiveLeaf();
    void resizeOnMainWindowEmitsResizeSignal();
    void popoutAndReparentEmitWindowFrameChange();
};

void TestWorkspaceActiveLeafRouter::layoutReadyDefaultsTrueAfterConstruction()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-default"), &registry);
    QVERIFY(ws.isLayoutReady());
}

void TestWorkspaceActiveLeafRouter::sameLeafSetTwiceDoesNotRefire()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-identity"), &registry);
    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);

    QSignalSpy spy(&ws, &Workspace::activeLeafChanged);
    ws.setActiveLeaf(leaf);
    ws.setActiveLeaf(leaf);
    QCOMPARE(spy.count(), 1);
}

void TestWorkspaceActiveLeafRouter::layoutNotReadySuppressesActiveLeafChange()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-suppress"), &registry);
    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);

    ws.setLayoutReady(false);
    QSignalSpy spy(&ws, &Workspace::activeLeafChanged);
    ws.setActiveLeaf(leaf);
    QCOMPARE(spy.count(), 0);
    QVERIFY(ws.activeLeaf() != leaf);
}

void TestWorkspaceActiveLeafRouter::layoutBecomesReadyAfterDeferredSet()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-deferred"), &registry);
    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);

    ws.setLayoutReady(false);
    QSignalSpy spy(&ws, &Workspace::activeLeafChanged);
    ws.setActiveLeaf(leaf);    // suppressed
    QCOMPARE(spy.count(), 0);

    ws.setLayoutReady(true);   // gate lifts
    ws.setActiveLeaf(leaf);    // fires now
    QCOMPARE(spy.count(), 1);
}

void TestWorkspaceActiveLeafRouter::setLayoutReadyFalseToTrueEmitsLayoutReadyOnce()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-emit"), &registry);

    QSignalSpy spy(&ws, &Workspace::layoutReady);
    ws.setLayoutReady(false);
    QCOMPARE(spy.count(), 0);
    ws.setLayoutReady(true);
    QCOMPARE(spy.count(), 1);
}

void TestWorkspaceActiveLeafRouter::setLayoutReadySameValueIsNoOp()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-noop"), &registry);

    QSignalSpy spy(&ws, &Workspace::layoutReady);
    // Default is true; setting true again is a no-op.
    ws.setLayoutReady(true);
    QCOMPARE(spy.count(), 0);

    ws.setLayoutReady(false);
    ws.setLayoutReady(false);
    QCOMPARE(spy.count(), 0);
}

void TestWorkspaceActiveLeafRouter::focusInsideLeafWidgetMarksLeafActive()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-focus"), &registry);
    ws.kddwMainWindow()->show();

    auto *first = ws.createLeafInActiveGroup();
    auto *second = ws.createLeafInActiveGroup();
    QVERIFY(first && second);

    // Programmatically activate the first leaf so the router has something
    // to *change* away from. Identity gate would otherwise swallow the
    // signal when the router's first walk lands on the same leaf.
    ws.setActiveLeaf(first);
    QCOMPARE(ws.activeLeaf(), first);

    // Embed a focusable child inside the second leaf's dock and give it
    // focus. The router walks the parent chain from the focused widget,
    // hits the leaf's dockWidget, and routes through Workspace::setActiveLeaf.
    auto *child = new QWidget(second->dockWidget());
    child->setFocusPolicy(Qt::StrongFocus);
    child->show();
    child->setFocus(Qt::OtherFocusReason);
    ws.kddwMainWindow()->activateWindow();
    QApplication::processEvents();

    QTRY_COMPARE(ws.activeLeaf(), second);
}

void TestWorkspaceActiveLeafRouter::focusOutsideMainWindowDoesNotChangeActiveLeaf()
{
    // Cluster L Phase L3, C4: onFocusChanged early-outs when the newly
    // focused widget isn't under the KDDW main window (e.g. a sidebar
    // toolview or a standalone dialog). Regression coverage: such a focus
    // move must not perturb the active leaf.
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-focus-outside"), &registry);
    ws.kddwMainWindow()->show();

    auto *first = ws.createLeafInActiveGroup();
    QVERIFY(first);
    ws.setActiveLeaf(first);
    QCOMPARE(ws.activeLeaf(), first);

    QWidget outside;
    outside.setFocusPolicy(Qt::StrongFocus);
    outside.show();
    outside.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();

    QCOMPARE(ws.activeLeaf(), first);
}

void TestWorkspaceActiveLeafRouter::resizeOnMainWindowEmitsResizeSignal()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-resize"), &registry);
    ws.kddwMainWindow()->show();

    QSignalSpy spy(&ws, &Workspace::resize);
    ws.kddwMainWindow()->resize(640, 480);
    QApplication::processEvents();
    ws.kddwMainWindow()->resize(800, 600);
    QApplication::processEvents();

    QVERIFY(spy.count() >= 1);
}

void TestWorkspaceActiveLeafRouter::popoutAndReparentEmitWindowFrameChange()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-windowframe"), &registry);
    ws.kddwMainWindow()->show();

    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);

    QSignalSpy spy(&ws, &Workspace::windowFrameChange);
    auto *win = ws.popoutLeaf(leaf);
    QVERIFY(win);
    QCOMPARE(spy.count(), 1);

    ws.reparentToMain(win);
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TestWorkspaceActiveLeafRouter)
#include "tst_workspace_active_leaf_router.moc"
