// tests/core/tst_leaf_undo.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include "corbomite/core/LeafHistory.h"
#include "corbomite/core/View.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

namespace {
// Minimal stub view that round-trips state + ephemeral state. Used to
// observe the restoration path through setViewState/setEphemeralState.
class StubUndoView : public View {
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("undo-stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }

    QJsonObject getState() const override { return m_state; }
    void setState(const QJsonObject &s) override { m_state = s; }
    QJsonObject getEphemeralState() const override { return m_eState; }
    void setEphemeralState(const QJsonObject &s) override { m_eState = s; }

    QJsonObject m_state;
    QJsonObject m_eState;
};
} // namespace

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

    void undoCloseRestoresLeafHistory()
    {
        // Audit: workspace.md §"Top gaps" — undoCloseLeaf does not restore
        // leafHistory or eState. The captured fields exist in UndoEntry but
        // the restore path only reads id/pinned/group/viewState.
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);

        // Seed back-stack so canGoBack() returns true.
        LeafHistoryEntry entry;
        entry.title = QStringLiteral("prev");
        entry.icon = QStringLiteral("document");
        entry.state = QJsonObject{{QStringLiteral("type"), QStringLiteral("undo-stub")}};
        leaf->history().push(entry);
        QVERIFY(leaf->history().canGoBack());

        ws.closeLeaf(leaf);
        ws.undoCloseLeaf();

        QCOMPARE(ws.allLeaves().size(), 1);
        auto *restored = ws.allLeaves().first();
        QVERIFY(restored->history().canGoBack());
    }

    void undoCloseRestoresEphemeralState()
    {
        ViewRegistry registry;
        registry.registerView(QStringLiteral("undo-stub"),
            [](WorkspaceLeaf *l) -> View * { return new StubUndoView(l); });

        Workspace ws(&registry);
        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);

        leaf->setViewState(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("undo-stub")}});
        leaf->setEphemeralState(QJsonObject{
            {QStringLiteral("scrollY"), 1234}});
        QCOMPARE(leaf->getEphemeralState().value(QStringLiteral("scrollY")).toInt(),
                 1234);

        ws.closeLeaf(leaf);
        ws.undoCloseLeaf();

        QCOMPARE(ws.allLeaves().size(), 1);
        auto *restored = ws.allLeaves().first();
        QCOMPARE(restored->getEphemeralState().value(
                     QStringLiteral("scrollY")).toInt(),
                 1234);
    }

    void undoCloseRestoresOriginalContainer()
    {
        // Audit: workspace.md §"Medium severity" #7 — undoCloseLeaf
        // recreates the leaf in the active group, not the original tab
        // group. Closing a tab in pane B with active leaf in pane A should
        // undo back into pane B.
        ViewRegistry registry;
        Workspace ws(QStringLiteral("undo-container"), &registry);

        // Two-leaf tab group via split.
        auto *a = ws.createLeafInActiveGroup();
        QVERIFY(a);
        ws.setActiveLeaf(a);
        auto *b = ws.splitLeaf(a, Qt::Horizontal);
        QVERIFY(b);

        // Add a tab next to b in its group, then close that tab. The active
        // leaf is `a` (in the *other* group).
        auto *bSibling = ws.createLeafInGroupOf(b);
        QVERIFY(bSibling);
        QCOMPARE(ws.leafCountInGroup(b), 2);
        ws.setActiveLeaf(a);
        QCOMPARE(ws.leafCountInGroup(a), 1);

        ws.closeLeaf(bSibling);
        QCOMPARE(ws.leafCountInGroup(b), 1);
        QCOMPARE(ws.leafCountInGroup(a), 1);

        ws.undoCloseLeaf();

        // The restored leaf must rejoin b's group, not a's.
        QCOMPARE(ws.leafCountInGroup(b), 2);
        QCOMPARE(ws.leafCountInGroup(a), 1);
    }

    void undoClosePreservesLeafId()
    {
        // Verify findLeafById resolves to the restored leaf at its original
        // id — covers the m_leavesById rekey + KDDW uniqueName re-namespace.
        ViewRegistry registry;
        Workspace ws(QStringLiteral("undo-id"), &registry);

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        const QString originalId = leaf->id();

        ws.closeLeaf(leaf);
        QVERIFY(ws.findLeafById(originalId) == nullptr);

        ws.undoCloseLeaf();
        auto *restored = ws.findLeafById(originalId);
        QVERIFY(restored != nullptr);
        QCOMPARE(restored->id(), originalId);
    }
};

QTEST_MAIN(TestLeafUndo)
#include "tst_leaf_undo.moc"
