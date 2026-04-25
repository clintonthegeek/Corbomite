// tests/core/tst_workspace_active_leaf_router.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 6: layoutReady gate + WorkspaceActiveLeafRouter behaviours.
// Phase 6.2 lights up the gate-related slots (this file). Phase 6.1 will add
// focus-driven activation slots once the WorkspaceActiveLeafRouter class lands.

#include <QSignalSpy>
#include <QTest>

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

QTEST_MAIN(TestWorkspaceActiveLeafRouter)
#include "tst_workspace_active_leaf_router.moc"
