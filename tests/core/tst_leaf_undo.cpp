// tests/core/tst_leaf_undo.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestLeafUndo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void undoHistoryInitiallyEmpty()
    {
        ViewRegistry registry;
        Workspace ws(&registry);
        QVERIFY(!ws.canUndoCloseLeaf());
    }

    void closeLeafPushesUndo()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf);
        ws.setActiveLeaf(leaf);

        ws.closeLeaf(leaf);
        QVERIFY(ws.canUndoCloseLeaf());
    }

    void undoCloseRestoresLeaf()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf);
        ws.setActiveLeaf(leaf);

        ws.closeLeaf(leaf);
        QCOMPARE(tabs->childCount(), 0);

        ws.undoCloseLeaf();
        QCOMPARE(tabs->childCount(), 1);
    }

    void undoCapAt10()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));

        for (int i = 0; i < 12; ++i) {
            auto *leaf = new WorkspaceLeaf(&registry);
            tabs->addChild(leaf);
            ws.setActiveLeaf(leaf);
            ws.closeLeaf(leaf);
        }

        int count = 0;
        while (ws.canUndoCloseLeaf()) {
            ws.undoCloseLeaf();
            ++count;
        }
        QCOMPARE(count, 10);
    }
};

QTEST_MAIN(TestLeafUndo)
#include "tst_leaf_undo.moc"
