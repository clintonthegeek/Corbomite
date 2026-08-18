// tests/core/tst_workspace_containers.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 5: WorkspaceFloating + Workspace::floatingSplit
// accessor. The WorkspaceContainer/WorkspaceRoot/WorkspaceSidedock
// bookkeeping shells this file used to also cover (Phase 7.5) were
// deleted in Cluster L Phase L3 (C1) — dead weight with no real
// callers; `leftSplit()`/`rightSplit()` returned literal `nullptr`,
// a crash invitation rather than genuine Obsidian-shape compatibility.

#include <QSignalSpy>
#include <QTest>

#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceFloating.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceWindow.h"

using namespace Corbomite;

class TestWorkspaceContainers : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void floatingSplit_tracksWindowsList();
};

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

QTEST_MAIN(TestWorkspaceContainers)
#include "tst_workspace_containers.moc"
