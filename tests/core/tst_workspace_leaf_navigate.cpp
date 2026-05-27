// tests/core/tst_workspace_leaf_navigate.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spec-driven tests for WorkspaceLeaf navigation and LeafHistory.
// Claims verified:
//   - navigate(viewState) pushes current state to history, then loads new state
//   - goBack() restores previous state from history
//   - goForward() re-navigates to a state you went back from
//   - History is capped at 20 entries (LeafHistory::Cap)
//   - canGoBack() / canGoForward() reflect history state accurately
//   - goBack() with no history is a no-op (does not crash)
//   - After navigate → goBack → goForward, view state matches the original forward state
//   - push() clears the forward stack
//
// Reference: docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md §5 "LeafHistory"

#include <QtTest/QtTest>
#include <QApplication>
#include <QJsonObject>

#include "corbomite/core/LeafHistory.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"

using namespace Corbomite;

// ---------------------------------------------------------------------------
// Minimal stub View so we can construct WorkspaceLeaf with a real view
// ---------------------------------------------------------------------------
class StubView : public View
{
    Q_OBJECT
public:
    explicit StubView(WorkspaceLeaf *leaf, QWidget *parent = nullptr)
        : View(leaf, parent)
    {}
    QString getViewType()    const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }

    // Track the last setState call so we can verify navigate loaded the new state
    QJsonObject m_state;
    void setState(const QJsonObject &state) override { m_state = state; }
    QJsonObject getState() const override { return m_state; }
};

// A second view type, so we can reproduce cross-view-type back/forward
// navigation (e.g. a ".base" BasesView → a wikilinked markdown note → back).
class StubView2 : public View
{
    Q_OBJECT
public:
    explicit StubView2(WorkspaceLeaf *leaf, QWidget *parent = nullptr)
        : View(leaf, parent)
    {}
    QString getViewType()    const override { return QStringLiteral("stub2"); }
    QString getDisplayText() const override { return QStringLiteral("Stub2"); }

    QJsonObject m_state;
    void setState(const QJsonObject &state) override { m_state = state; }
    QJsonObject getState() const override { return m_state; }
};

// ---------------------------------------------------------------------------
// Helper: build a simple ViewState object with a "file" key
// ---------------------------------------------------------------------------
static QJsonObject makeState(const QString &file)
{
    QJsonObject s;
    s[QStringLiteral("file")] = file;
    return s;
}

// ---------------------------------------------------------------------------
// Helper: build a LeafHistoryEntry
// ---------------------------------------------------------------------------
static LeafHistoryEntry makeEntry(const QString &title, const QString &file)
{
    LeafHistoryEntry e;
    e.title = title;
    e.icon  = QStringLiteral("document");
    e.state = makeState(file);
    return e;
}

// ===========================================================================
//  LeafHistory unit tests (direct API — no WorkspaceLeaf needed)
// ===========================================================================
class TestLeafHistory : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // Initial state: both canGoBack and canGoForward are false
    // -----------------------------------------------------------------------
    void initialStateEmpty()
    {
        LeafHistory h;
        QVERIFY(!h.canGoBack());
        QVERIFY(!h.canGoForward());
    }

    // -----------------------------------------------------------------------
    // After push(): canGoBack() is true, canGoForward() is still false
    // -----------------------------------------------------------------------
    void pushEnablesBack()
    {
        LeafHistory h;
        h.push(makeEntry("Note A", "a.md"));
        QVERIFY(h.canGoBack());
        QVERIFY(!h.canGoForward());
    }

    // -----------------------------------------------------------------------
    // goBack() returns the previously pushed entry
    // -----------------------------------------------------------------------
    void goBackReturnsPushedEntry()
    {
        LeafHistory h;
        h.push(makeEntry("Note A", "a.md"));

        LeafHistoryEntry current = makeEntry("Note B", "b.md");
        LeafHistoryEntry restored = h.goBack(current);

        QCOMPARE(restored.state[QStringLiteral("file")].toString(),
                 QStringLiteral("a.md"));
        QCOMPARE(restored.title, QStringLiteral("Note A"));
    }

    // -----------------------------------------------------------------------
    // After goBack(): canGoForward() becomes true, canGoBack() may be false
    // -----------------------------------------------------------------------
    void goBackEnablesForward()
    {
        LeafHistory h;
        h.push(makeEntry("Note A", "a.md"));

        LeafHistoryEntry current = makeEntry("Note B", "b.md");
        h.goBack(current);

        QVERIFY(!h.canGoBack());
        QVERIFY(h.canGoForward());
    }

    // -----------------------------------------------------------------------
    // goForward() returns the entry we passed when calling goBack()
    // i.e. the "current" state at the time of the back navigation
    // -----------------------------------------------------------------------
    void goForwardRestoresForwardEntry()
    {
        LeafHistory h;
        h.push(makeEntry("Note A", "a.md"));

        // current=B, go back → A restored, B goes to forward stack
        LeafHistoryEntry noteB = makeEntry("Note B", "b.md");
        h.goBack(noteB);

        // Now go forward: B should come back
        LeafHistoryEntry noteA = makeEntry("Note A", "a.md");
        LeafHistoryEntry restored = h.goForward(noteA);

        QCOMPARE(restored.state[QStringLiteral("file")].toString(),
                 QStringLiteral("b.md"));
        QCOMPARE(restored.title, QStringLiteral("Note B"));
    }

    // -----------------------------------------------------------------------
    // Round-trip: push(A) → goBack(current=B) → goForward(current=A) restores B
    // Spec: "After navigate → goBack → goForward, the view state matches
    //        the original forward state"
    // -----------------------------------------------------------------------
    void navigateBackForwardRoundTrip()
    {
        LeafHistory h;

        LeafHistoryEntry entryA = makeEntry("Note A", "a.md");
        h.push(entryA); // current was A, pushed to back; now at B

        LeafHistoryEntry entryB = makeEntry("Note B", "b.md");
        LeafHistoryEntry restored = h.goBack(entryB); // B→forward, A←back
        QCOMPARE(restored.state[QStringLiteral("file")].toString(),
                 QStringLiteral("a.md"));

        // Go forward: A→back, B←forward
        LeafHistoryEntry restoredFwd = h.goForward(restored);
        QCOMPARE(restoredFwd.state[QStringLiteral("file")].toString(),
                 QStringLiteral("b.md"));
        QCOMPARE(restoredFwd.title, QStringLiteral("Note B"));
    }

    // -----------------------------------------------------------------------
    // push() clears the forward stack
    // Spec: "back.push(current), forward.clear(), enforce cap"
    // -----------------------------------------------------------------------
    void pushClearsForwardStack()
    {
        LeafHistory h;

        h.push(makeEntry("Note A", "a.md"));
        LeafHistoryEntry entryB = makeEntry("Note B", "b.md");
        h.goBack(entryB);         // creates a forward entry
        QVERIFY(h.canGoForward());

        // New push must clear the forward stack
        h.push(makeEntry("Note C", "c.md"));
        QVERIFY(!h.canGoForward());
    }

    // -----------------------------------------------------------------------
    // LeafHistory::Cap is exactly 20
    // Spec: "History is capped at 20 entries (LeafHistory::Cap)"
    // -----------------------------------------------------------------------
    void capConstantIs20()
    {
        QCOMPARE(LeafHistory::Cap, 20);
    }

    // -----------------------------------------------------------------------
    // Back stack never exceeds Cap entries
    // -----------------------------------------------------------------------
    void backStackCapEnforced()
    {
        LeafHistory h;

        // Push Cap + 5 entries
        for (int i = 0; i < LeafHistory::Cap + 5; ++i) {
            h.push(makeEntry(QStringLiteral("Note %1").arg(i),
                             QStringLiteral("note%1.md").arg(i)));
        }

        // Walk back to count how many entries were stored
        int count = 0;
        LeafHistoryEntry current = makeEntry("current", "current.md");
        while (h.canGoBack()) {
            current = h.goBack(current);
            ++count;
            // Safety: break if implementation has a bug that doesn't terminate
            if (count > LeafHistory::Cap + 10)
                break;
        }
        QCOMPARE(count, LeafHistory::Cap);
    }

    // -----------------------------------------------------------------------
    // canGoBack() / canGoForward() are accurate through multi-step sequences
    // -----------------------------------------------------------------------
    void canGoBackForwardReflectsState()
    {
        LeafHistory h;
        QVERIFY(!h.canGoBack());
        QVERIFY(!h.canGoForward());

        h.push(makeEntry("A", "a.md"));
        QVERIFY(h.canGoBack());
        QVERIFY(!h.canGoForward());

        h.push(makeEntry("B", "b.md"));
        QVERIFY(h.canGoBack());
        QVERIFY(!h.canGoForward());

        LeafHistoryEntry cur = makeEntry("C", "c.md");
        cur = h.goBack(cur);  // back to B; C in forward
        QVERIFY(h.canGoBack());
        QVERIFY(h.canGoForward());

        cur = h.goBack(cur);  // back to A; B,C in forward
        QVERIFY(!h.canGoBack());
        QVERIFY(h.canGoForward());

        cur = h.goForward(cur);  // forward to B
        QVERIFY(h.canGoBack());
        QVERIFY(h.canGoForward());

        cur = h.goForward(cur);  // forward to C
        QVERIFY(h.canGoBack());
        QVERIFY(!h.canGoForward());
    }

    // -----------------------------------------------------------------------
    // Serialize / deserialize round-trip preserves canGoBack/canGoForward
    // -----------------------------------------------------------------------
    void serializeRoundTrip()
    {
        LeafHistory h;
        h.push(makeEntry("Note A", "a.md"));
        h.push(makeEntry("Note B", "b.md"));

        // Go back once so there is something in both stacks
        LeafHistoryEntry cur = makeEntry("Note C", "c.md");
        h.goBack(cur);

        QJsonObject json = h.serialize();
        LeafHistory h2 = LeafHistory::deserialize(json);

        QCOMPARE(h2.canGoBack(),    h.canGoBack());
        QCOMPARE(h2.canGoForward(), h.canGoForward());
    }
};

// ===========================================================================
//  WorkspaceLeaf navigation integration tests
// ===========================================================================
class TestWorkspaceLeafNavigate : public QObject
{
    Q_OBJECT

private:
    ViewRegistry  *m_registry = nullptr;
    WorkspaceLeaf *m_leaf     = nullptr;
    StubView      *m_view     = nullptr; // owned by m_leaf after open()

    void setUp()
    {
        m_registry = new ViewRegistry(this);
        m_registry->registerView(QStringLiteral("stub"),
            [](WorkspaceLeaf *leaf) -> View * {
                return new StubView(leaf);
            });
        m_registry->registerView(QStringLiteral("stub2"),
            [](WorkspaceLeaf *leaf) -> View * {
                return new StubView2(leaf);
            });
        m_leaf = new WorkspaceLeaf(m_registry, this);

        // Create and open a stub view so navigate() can call setState
        m_view = new StubView(m_leaf);
        m_leaf->open(m_view);
        m_view->setState(makeState("initial.md"));
    }

    void tearDown()
    {
        delete m_leaf;
        m_leaf = nullptr;
        m_view = nullptr; // owned (and deleted) by m_leaf
        delete m_registry;
        m_registry = nullptr;
    }

private Q_SLOTS:

    void init()    { setUp(); }
    void cleanup() { tearDown(); }

    // -----------------------------------------------------------------------
    // navigate() loads the new state into the current view
    // -----------------------------------------------------------------------
    void navigateLoadsNewState()
    {
        m_leaf->navigate(makeState("new.md"));
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("new.md"));
    }

    // -----------------------------------------------------------------------
    // navigate() pushes the current state to history before loading the new one
    // Spec: "pushes current state to history, then loads new state"
    // -----------------------------------------------------------------------
    void navigatePushesCurrentStateToHistory()
    {
        QVERIFY(!m_leaf->history().canGoBack());

        m_leaf->navigate(makeState("new.md"));

        QVERIFY(m_leaf->history().canGoBack());
    }

    // -----------------------------------------------------------------------
    // goBack() restores the previously navigated-away-from state
    // Spec: "goBack() restores previous state from history"
    // -----------------------------------------------------------------------
    void goBackRestoresPreviousState()
    {
        // Set initial state to a.md, navigate to b.md
        m_view->setState(makeState("a.md"));
        m_leaf->navigate(makeState("b.md"));
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("b.md"));

        m_leaf->goBack();

        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("a.md"));
    }

    // -----------------------------------------------------------------------
    // goForward() re-navigates to the state you went back from
    // Spec: "goForward() re-navigates to a state you went back from"
    // -----------------------------------------------------------------------
    void goForwardReNavigatesToForwardState()
    {
        m_view->setState(makeState("a.md"));
        m_leaf->navigate(makeState("b.md"));
        m_leaf->goBack();

        QVERIFY(m_leaf->history().canGoForward());
        m_leaf->goForward();

        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("b.md"));
    }

    // -----------------------------------------------------------------------
    // Full round-trip: navigate → goBack → goForward → same forward state
    // Spec: "After navigate → goBack → goForward, the view state matches
    //        the original forward state"
    // -----------------------------------------------------------------------
    void navigateBackForwardRoundTrip()
    {
        m_view->setState(makeState("a.md"));
        m_leaf->navigate(makeState("b.md"));

        m_leaf->goBack();
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("a.md"));

        m_leaf->goForward();
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("b.md"));
    }

    // -----------------------------------------------------------------------
    // goBack() with no history is a no-op — must not crash
    // Spec: "goBack() with no history is a no-op (does not crash)"
    // -----------------------------------------------------------------------
    void goBackNoHistoryIsNoOp()
    {
        QVERIFY(!m_leaf->history().canGoBack());
        m_view->setState(makeState("current.md"));

        // Must not crash
        m_leaf->goBack();

        // State must be unchanged
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("current.md"));
    }

    // -----------------------------------------------------------------------
    // goForward() with no forward history is a no-op — must not crash
    // -----------------------------------------------------------------------
    void goForwardNoHistoryIsNoOp()
    {
        QVERIFY(!m_leaf->history().canGoForward());
        m_view->setState(makeState("current.md"));

        // Must not crash
        m_leaf->goForward();

        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("current.md"));
    }

    // -----------------------------------------------------------------------
    // canGoBack() / canGoForward() accurately reflect leaf navigation state
    // -----------------------------------------------------------------------
    void canGoBackForwardAccurate()
    {
        QVERIFY(!m_leaf->history().canGoBack());
        QVERIFY(!m_leaf->history().canGoForward());

        m_view->setState(makeState("a.md"));
        m_leaf->navigate(makeState("b.md"));

        QVERIFY(m_leaf->history().canGoBack());
        QVERIFY(!m_leaf->history().canGoForward());

        m_leaf->goBack();
        QVERIFY(!m_leaf->history().canGoBack());
        QVERIFY(m_leaf->history().canGoForward());

        m_leaf->goForward();
        QVERIFY(m_leaf->history().canGoBack());
        QVERIFY(!m_leaf->history().canGoForward());
    }

    // -----------------------------------------------------------------------
    // Multiple navigations build up the back stack in order
    // -----------------------------------------------------------------------
    void multipleNavigationsStackCorrectly()
    {
        m_view->setState(makeState("a.md"));
        m_leaf->navigate(makeState("b.md"));
        m_leaf->navigate(makeState("c.md"));
        m_leaf->navigate(makeState("d.md"));

        // d.md is current; a, b, c are in back stack
        QVERIFY(m_leaf->history().canGoBack());
        QVERIFY(!m_leaf->history().canGoForward());

        m_leaf->goBack();
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("c.md"));

        m_leaf->goBack();
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("b.md"));

        m_leaf->goBack();
        QCOMPARE(m_view->getState()[QStringLiteral("file")].toString(),
                 QStringLiteral("a.md"));

        QVERIFY(!m_leaf->history().canGoBack());
    }

    // -----------------------------------------------------------------------
    // Navigating from a back-stepped position clears the forward branch
    // (branching navigation discards what was ahead)
    // -----------------------------------------------------------------------
    void newNavigateFromBackClearsForwardBranch()
    {
        m_view->setState(makeState("a.md"));
        m_leaf->navigate(makeState("b.md"));
        m_leaf->goBack();
        QVERIFY(m_leaf->history().canGoForward());

        // Navigate to a new page from "a" — forward stack should be cleared
        m_leaf->navigate(makeState("c.md"));
        QVERIFY(!m_leaf->history().canGoForward());
    }

    // -----------------------------------------------------------------------
    // goBack() across DIFFERENT view types must restore the original view
    // *type*, not just push the old state onto the current (wrong) view.
    // Regression: clicking a wikilink in a ".base" view navigates to a
    // markdown note (recreating the leaf's view as markdown); pressing Back
    // used to call setState() on the still-mounted markdown view, so the
    // ".base" file rendered as raw markdown text instead of the table view.
    // -----------------------------------------------------------------------
    void goBackRestoresViewTypeAcrossViewTypes()
    {
        m_view->setState(makeState("films.base"));  // current view is "stub"

        // Forward-navigate to a different view type (like base → markdown note).
        QJsonObject toStub2;
        toStub2[QStringLiteral("type")]  = QStringLiteral("stub2");
        toStub2[QStringLiteral("state")] = makeState("note.md");
        m_leaf->navigate(toStub2);
        QCOMPARE(m_leaf->getViewState()[QStringLiteral("type")].toString(),
                 QStringLiteral("stub2"));

        // Back must recreate the ORIGINAL view type, with its state.
        m_leaf->goBack();
        QCOMPARE(m_leaf->getViewState()[QStringLiteral("type")].toString(),
                 QStringLiteral("stub"));
        QCOMPARE(m_leaf->getViewState()[QStringLiteral("state")]
                     .toObject()[QStringLiteral("file")].toString(),
                 QStringLiteral("films.base"));
    }

    // -----------------------------------------------------------------------
    // goForward() across different view types likewise restores the forward
    // entry's view type.
    // -----------------------------------------------------------------------
    void goForwardRestoresViewTypeAcrossViewTypes()
    {
        m_view->setState(makeState("films.base"));

        QJsonObject toStub2;
        toStub2[QStringLiteral("type")]  = QStringLiteral("stub2");
        toStub2[QStringLiteral("state")] = makeState("note.md");
        m_leaf->navigate(toStub2);
        m_leaf->goBack();   // back to stub / films.base
        QCOMPARE(m_leaf->getViewState()[QStringLiteral("type")].toString(),
                 QStringLiteral("stub"));

        m_leaf->goForward();  // forward again to stub2 / note.md
        QCOMPARE(m_leaf->getViewState()[QStringLiteral("type")].toString(),
                 QStringLiteral("stub2"));
        QCOMPARE(m_leaf->getViewState()[QStringLiteral("state")]
                     .toObject()[QStringLiteral("file")].toString(),
                 QStringLiteral("note.md"));
    }
};

// ---------------------------------------------------------------------------
// Multi-class test runner
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    int result = 0;

    {
        TestLeafHistory t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestWorkspaceLeafNavigate t;
        result |= QTest::qExec(&t, argc, argv);
    }

    return result;
}

#include "tst_workspace_leaf_navigate.moc"
