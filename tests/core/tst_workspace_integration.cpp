// tests/core/tst_workspace_integration.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Workspace public-API behaviour tests.
//
// History: this file used to assert claims of the Cluster-G `WorkspaceSplit`/
// `WorkspaceTabs`/`WorkspaceItem`/`WorkspaceParent` substrate (widget hierarchy,
// tab-bar mechanics, parent/child signals, etc.). Cluster Y Phase 4 demotes
// that substrate to internal-only (KDDW takes over in Phase 4b), so the
// substrate-poking tests were dropped. The kept tests exercise behaviour
// reachable from `Workspace`'s public surface — what plugin authors and
// `MainWindow` actually consume.

#include <QApplication>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

using namespace Corbomite;

Q_DECLARE_METATYPE(Corbomite::WorkspaceLeaf *)

class WorkspaceIntegrationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void test_workspace_createLeafInActiveGroup_addsLeaf()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        const int before = ws.allLeaves().size();
        WorkspaceLeaf *leaf = ws.createLeafInActiveGroup();

        QVERIFY2(leaf != nullptr,
                 "createLeafInActiveGroup must return a non-null leaf");
        QCOMPARE(ws.allLeaves().size(), before + 1);
        QVERIFY(ws.allLeaves().contains(leaf));
    }

    void test_workspace_closeLeaf_removesLeaf()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceLeaf *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf != nullptr);

        const int before = ws.allLeaves().size();
        ws.closeLeaf(leaf);

        QCOMPARE(ws.allLeaves().size(), before - 1);
        QVERIFY(!ws.allLeaves().contains(leaf));
    }

    void test_workspace_closeLeaf_emitsSignal()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceLeaf *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf != nullptr);

        QSignalSpy spy(&ws, &Workspace::leafClosed);
        ws.closeLeaf(leaf);

        QCOMPARE(spy.count(), 1);
    }

    void test_workspace_duplicateLeaf_returnsNonNullLeaf()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        auto *src = ws.createLeafInActiveGroup();
        QVERIFY(src);

        auto *dup = ws.duplicateLeaf(src, Qt::Horizontal);
        QVERIFY2(dup != nullptr, "duplicateLeaf must return a non-null new leaf");
        QVERIFY(dup != src);
    }

    void test_workspace_duplicateLeaf_newLeafIsActive()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        auto *src = ws.createLeafInActiveGroup();
        QVERIFY(src);
        ws.setActiveLeaf(src);

        auto *dup = ws.duplicateLeaf(src, Qt::Horizontal);
        QVERIFY(dup);
        QCOMPARE(ws.activeLeaf(), dup);
    }

    void test_workspace_duplicateLeaf_clonesPinnedAndGroup()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        auto *src = ws.createLeafInActiveGroup();
        QVERIFY(src);
        src->setPinned(true);
        src->setGroup(QStringLiteral("group-A"));

        auto *dup = ws.duplicateLeaf(src, Qt::Vertical);
        QVERIFY(dup);
        QVERIFY2(dup->pinned(), "duplicated leaf must carry pinned flag");
        QCOMPARE(dup->group(), QStringLiteral("group-A"));
    }

    void test_workspace_duplicateLeaf_addsExactlyOneLeaf()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        auto *src = ws.createLeafInActiveGroup();
        QVERIFY(src);

        const int before = ws.allLeaves().size();
        auto *dup = ws.duplicateLeaf(src, Qt::Horizontal);
        QVERIFY(dup);

        QCOMPARE(ws.allLeaves().size(), before + 1);
        QVERIFY(ws.allLeaves().contains(src));
        QVERIFY(ws.allLeaves().contains(dup));
    }

    void test_workspace_allLeaves_collectsAll()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceLeaf *l1 = ws.createLeafInActiveGroup();
        WorkspaceLeaf *l2 = ws.createLeafInGroupOf(l1);
        WorkspaceLeaf *l3 = ws.createLeafInGroupOf(l1);

        QVector<WorkspaceLeaf *> all = ws.allLeaves();
        QVERIFY(all.contains(l1));
        QVERIFY(all.contains(l2));
        QVERIFY(all.contains(l3));
        QCOMPARE(all.size(), 3);
    }

    void test_workspace_findLeafById()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceLeaf *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf != nullptr);

        QString leafId = leaf->id();
        WorkspaceLeaf *found = ws.findLeafById(leafId);
        QCOMPARE(found, leaf);
    }

    void test_workspace_findLeafById_unknownId_returnsNull()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceLeaf *found = ws.findLeafById(QStringLiteral("0000000000000000"));
        QVERIFY(found == nullptr);
    }
};

QTEST_MAIN(WorkspaceIntegrationTest)
#include "tst_workspace_integration.moc"
