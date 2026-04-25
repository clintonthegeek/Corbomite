// tests/core/tst_workspace_factory.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 7: Obsidian-shape Workspace::getLeaf(mode, dir)
// factory and Workspace::openLinkText(...) dispatcher.

#include <QPointer>
#include <QTest>

#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

using namespace Corbomite;

class TestWorkspaceFactory : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void getLeaf_sameMode_returnsActive();
    void getLeaf_sameMode_withNoActive_createsLeaf();
    void getLeaf_tabMode_createsSiblingTab();
    void getLeaf_splitMode_createsSibling();
    void getLeaf_windowMode_createsFloating();
};

void TestWorkspaceFactory::getLeaf_sameMode_returnsActive()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-same"), &registry);

    auto *seed = ws.createLeafInActiveGroup();
    QVERIFY(seed);
    ws.setActiveLeaf(seed);

    auto *got = ws.getLeaf(Workspace::LeafMode::Same);
    QCOMPARE(got, seed);
    QCOMPARE(ws.allLeaves().size(), 1);
}

void TestWorkspaceFactory::getLeaf_sameMode_withNoActive_createsLeaf()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-same-empty"), &registry);

    QVERIFY(ws.activeLeaf() == nullptr);
    auto *got = ws.getLeaf(Workspace::LeafMode::Same);
    QVERIFY(got != nullptr);
    QCOMPARE(ws.allLeaves().size(), 1);
}

void TestWorkspaceFactory::getLeaf_tabMode_createsSiblingTab()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-tab"), &registry);

    auto *seed = ws.createLeafInActiveGroup();
    QVERIFY(seed);
    ws.setActiveLeaf(seed);

    auto *got = ws.getLeaf(Workspace::LeafMode::Tab);
    QVERIFY(got != nullptr);
    QVERIFY(got != seed);
    // Tab mode = same group as the active leaf.
    QCOMPARE(ws.leafCountInGroup(seed), 2);
    QCOMPARE(ws.leafCountInGroup(got), 2);
}

void TestWorkspaceFactory::getLeaf_splitMode_createsSibling()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-split"), &registry);

    auto *seed = ws.createLeafInActiveGroup();
    QVERIFY(seed);
    ws.setActiveLeaf(seed);

    auto *got = ws.getLeaf(Workspace::LeafMode::Split,
                            Workspace::LeafDirection::Horizontal);
    QVERIFY(got != nullptr);
    QVERIFY(got != seed);
    // Split mode = new tab group.
    QCOMPARE(ws.leafCountInGroup(seed), 1);
    QCOMPARE(ws.leafCountInGroup(got), 1);
    QCOMPARE(ws.allLeaves().size(), 2);
}

void TestWorkspaceFactory::getLeaf_windowMode_createsFloating()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-window"), &registry);

    // KDDW only spawns FloatingWindows once the host MainWindow is realised;
    // mirror tst_workspace_popout.cpp setup.
    ws.kddwMainWindow()->show();

    const int floatsBefore =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();

    auto *got = ws.getLeaf(Workspace::LeafMode::Window);
    QVERIFY(got != nullptr);

    const int floatsAfter =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();
    QCOMPARE(floatsAfter, floatsBefore + 1);
}

QTEST_MAIN(TestWorkspaceFactory)
#include "tst_workspace_factory.moc"
