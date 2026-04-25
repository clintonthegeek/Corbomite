// tests/core/tst_workspace_popout.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 5: popout windows atop KDDW FloatingWindow.
// Phase 5.1 lights only popoutLeaf_createsFloatingWindow; the remaining
// slots are populated in 5.3 (close-window propagation) and 5.4
// (geometry + maximize round-trip).

#include <QSignalSpy>
#include <QTest>

#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceWindow.h"

using namespace Corbomite;

class TestWorkspacePopout : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void popoutLeaf_createsFloatingWindow();
    void closeFloatingWindow_closesChildrenLeaves();
    void restoreFloatingWindow_preservesGeometry();
    void restoreFloatingWindow_preservesMaximize();
};

void TestWorkspacePopout::popoutLeaf_createsFloatingWindow()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-popout-create"), &registry);

    // KDDW only spawns FloatingWindows once the host MainWindow is realised;
    // without show() setFloating(true) silently no-ops. See the comment on
    // materializeFloatingWindow in WorkspaceSerializer.cpp.
    ws.kddwMainWindow()->show();

    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);
    QVERIFY(leaf->dockWidget());

    const int floatsBefore =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();

    auto *win = ws.popoutLeaf(leaf);
    QVERIFY(win != nullptr);

    const int floatsAfter =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();
    QCOMPARE(floatsAfter, floatsBefore + 1);
    QVERIFY(leaf->dockWidget()->isFloating());
}

void TestWorkspacePopout::closeFloatingWindow_closesChildrenLeaves()
{
    QSKIP("Wired up in Phase 5.3");
}

void TestWorkspacePopout::restoreFloatingWindow_preservesGeometry()
{
    QSKIP("Wired up in Phase 5.4");
}

void TestWorkspacePopout::restoreFloatingWindow_preservesMaximize()
{
    QSKIP("Wired up in Phase 5.4");
}

QTEST_MAIN(TestWorkspacePopout)
#include "tst_workspace_popout.moc"
