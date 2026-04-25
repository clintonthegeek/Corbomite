// tests/core/tst_leaf_undo.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/Workspace.h"
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

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        ws.setActiveLeaf(leaf);

        ws.closeLeaf(leaf);
        QVERIFY(ws.canUndoCloseLeaf());
    }

    void undoCloseRestoresLeaf()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        ws.setActiveLeaf(leaf);

        const int beforeClose = ws.allLeaves().size();
        ws.closeLeaf(leaf);
        QCOMPARE(ws.allLeaves().size(), beforeClose - 1);

        ws.undoCloseLeaf();
        QCOMPARE(ws.allLeaves().size(), beforeClose);
    }

    void undoCapAt10()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        for (int i = 0; i < 12; ++i) {
            auto *leaf = ws.createLeafInActiveGroup();
            QVERIFY(leaf);
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
