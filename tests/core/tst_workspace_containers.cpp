// tests/core/tst_workspace_containers.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 7.5: WorkspaceContainer / WorkspaceRoot /
// WorkspaceFloating / WorkspaceSidedock + Workspace::rootSplit /
// leftSplit / rightSplit / floatingSplit accessors.

#include <QSignalSpy>
#include <QTest>

#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceContainer.h"
#include "corbomite/core/WorkspaceFloating.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceRoot.h"
#include "corbomite/core/WorkspaceSidedock.h"
#include "corbomite/core/WorkspaceWindow.h"

using namespace Corbomite;

class TestWorkspaceContainers : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void rootSplit_isAlwaysPresent();
    void sideSplits_areStubbedNullptr();
    void floatingSplit_tracksWindowsList();
    void container_directionChange_emitsSignal();
    void sidedock_collapsedSizeAreSettable();
};

void TestWorkspaceContainers::rootSplit_isAlwaysPresent()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-root"), &registry);

    auto *root = ws.rootSplit();
    QVERIFY(root != nullptr);
    QCOMPARE(root->id(), QStringLiteral("root"));
    QCOMPARE(root->direction(), QStringLiteral("horizontal"));
}

void TestWorkspaceContainers::sideSplits_areStubbedNullptr()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-sides"), &registry);

    // Phase 7.5 ships these as stub accessors. Sidebars still live in
    // CorbomiteMDI, not the Workspace tree — these will become non-null
    // when a future cluster migrates them.
    QVERIFY(ws.leftSplit() == nullptr);
    QVERIFY(ws.rightSplit() == nullptr);
}

void TestWorkspaceContainers::floatingSplit_tracksWindowsList()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-floating"), &registry);
    ws.kddwMainWindow()->show();

    auto *floating = ws.floatingSplit();
    QVERIFY(floating != nullptr);
    QCOMPARE(floating->windows().size(), 0);

    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);
    auto *win = ws.popoutLeaf(leaf);
    QVERIFY(win != nullptr);
    QCOMPARE(floating->windows().size(), 1);
    QCOMPARE(floating->windows().first(), win);

    ws.reparentToMain(win);
    QCOMPARE(floating->windows().size(), 0);
}

void TestWorkspaceContainers::container_directionChange_emitsSignal()
{
    WorkspaceContainer c(QStringLiteral("c1"),
                          QStringLiteral("horizontal"));
    QSignalSpy spy(&c, &WorkspaceContainer::directionChanged);

    c.setDirection(QStringLiteral("horizontal"));
    QCOMPARE(spy.count(), 0);  // same value → no emit

    c.setDirection(QStringLiteral("vertical"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(c.direction(), QStringLiteral("vertical"));
}

void TestWorkspaceContainers::sidedock_collapsedSizeAreSettable()
{
    WorkspaceSidedock dock(QStringLiteral("left"),
                            WorkspaceSidedock::Side::Left);
    QCOMPARE(dock.side(), WorkspaceSidedock::Side::Left);
    QCOMPARE(dock.collapsed(), false);
    QCOMPARE(dock.size(), 0);

    QSignalSpy collapsedSpy(&dock, &WorkspaceSidedock::collapsedChanged);
    QSignalSpy sizeSpy(&dock, &WorkspaceSidedock::sizeChanged);

    dock.setCollapsed(true);
    QCOMPARE(collapsedSpy.count(), 1);
    QVERIFY(dock.collapsed());

    dock.setSize(280);
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(dock.size(), 280);
}

QTEST_MAIN(TestWorkspaceContainers)
#include "tst_workspace_containers.moc"
