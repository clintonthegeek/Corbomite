// tests/core/tst_workspace_linked_pane.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O5.T5 (Cluster I Phase 5 tail) — the two linked-pane and
// tab-reorder primitives `Workspace` gained, tested at the Workspace level
// rather than through MainWindow's QActions. What matters is the *policy*:
// which group id gets reused, when pinning propagates, what happens to a
// half-cleared group, and whether the ends of a tab bar actually clamp.
//
// Reference behaviour is the reverse-engineered Obsidian semantics recorded
// in docs/obsidian-audit: `workspace.md:123` (setGroupMember creates or
// REUSES the target's group id; setGroup fires group-change) and `:392`
// (setGroup joining a pinned group forces the new member pinned).
#include <QTest>

#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

using namespace Corbomite;

namespace {
// Three tabs in one group, plus the leaf under test.
struct Fixture {
    ViewRegistry registry;
    Workspace ws{QStringLiteral("linked-pane"), &registry};
    WorkspaceLeaf *a = nullptr;
    WorkspaceLeaf *b = nullptr;
    WorkspaceLeaf *c = nullptr;

    void build() {
        a = ws.createLeafInActiveGroup();
        QVERIFY(a);
        ws.setActiveLeaf(a);
        b = ws.createLeafInGroupOf(a);
        QVERIFY(b);
        c = ws.createLeafInGroupOf(b);
        QVERIFY(c);
    }
};
} // namespace

class TestWorkspaceLinkedPane : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- moveLeafWithinGroup -------------------------------------------

    // The happy path: the moved leaf's reported index really changes, and
    // its old neighbour is displaced — a no-op stub would leave indices
    // untouched.
    void moveRightSwapsWithNeighbour() {
        Fixture f;
        f.build();
        QCOMPARE(f.ws.leafIndexInGroup(f.a), 0);

        QVERIFY(f.ws.moveLeafWithinGroup(f.a, +1));
        QCOMPARE(f.ws.leafIndexInGroup(f.a), 1);
        QCOMPARE(f.ws.leafIndexInGroup(f.b), 0);
        QCOMPARE(f.ws.leafCountInGroup(f.a), 3);   // same group, still 3 tabs
    }

    void moveLeftSwapsWithNeighbour() {
        Fixture f;
        f.build();
        QVERIFY(f.ws.moveLeafWithinGroup(f.c, -1));
        QCOMPARE(f.ws.leafIndexInGroup(f.c), 1);
        QCOMPARE(f.ws.leafIndexInGroup(f.b), 2);
    }

    // Clamping is the contract ActionContextController's enablement depends
    // on: it disables the key at the ends, so the primitive must agree and
    // report "did not move" rather than wrapping or no-opping silently.
    void moveClampsAtTheEnds() {
        Fixture f;
        f.build();
        QVERIFY(!f.ws.moveLeafWithinGroup(f.a, -1));   // already leftmost
        QCOMPARE(f.ws.leafIndexInGroup(f.a), 0);
        QVERIFY(!f.ws.moveLeafWithinGroup(f.c, +1));   // already rightmost
        QCOMPARE(f.ws.leafIndexInGroup(f.c), 2);
    }

    void moveIsNoOpForNullAndZeroDelta() {
        Fixture f;
        f.build();
        QVERIFY(!f.ws.moveLeafWithinGroup(nullptr, +1));
        QVERIFY(!f.ws.moveLeafWithinGroup(f.b, 0));
        QCOMPARE(f.ws.leafIndexInGroup(f.b), 1);      // unchanged
    }

    // ---- linkLeaves: group-id reuse ------------------------------------

    // The documented rule is create-or-REUSE: linking to a leaf that already
    // has a group must adopt that exact id, not mint a second one. Asserting
    // equality (not just non-emptiness) is what makes this falsifiable.
    void linkReusesTargetsExistingGroupId() {
        Fixture f;
        f.build();
        f.a->setGroup(QStringLiteral("g-existing"));

        QVERIFY(f.ws.linkLeaves(f.b, f.a));
        QCOMPARE(f.a->group(), QStringLiteral("g-existing"));
        QCOMPARE(f.b->group(), QStringLiteral("g-existing"));
    }

    // With neither leaf grouped, a fresh id is minted and shared by both.
    void linkMintsOneSharedGroupIdWhenNeitherHasOne() {
        Fixture f;
        f.build();
        QVERIFY(f.ws.linkLeaves(f.b, f.c));
        const QString g = f.b->group();
        QVERIFY2(!g.isEmpty(), "link produced an empty group id");
        QCOMPARE(f.c->group(), g);
    }

    // A leaf that ALREADY belongs to a group must not orphan that group when
    // it links somewhere else. This is the case the audit's literal
    // "reuse the target's group id" wording gets wrong: linking a grouped
    // leaf to an ungrouped target under that reading mints a new id, moves
    // only the two leaves passed in, and leaves the third member pointing at
    // a group it is no longer part of. Pinned-independent, so it stays
    // covered even if the pin rules change.
    void linkingAGrouptLeafDoesNotOrphanItsGroup() {
        Fixture f;
        f.build();
        // Put a and b in one group.
        f.a->setGroup(QStringLiteral("g1"));
        f.ws.linkLeaves(f.a, f.b);
        QCOMPARE(f.ws.groupMembers(QStringLiteral("g1")).size(), 2);

        // Link that group (via a) to an ungrouped c.
        QVERIFY(f.ws.linkLeaves(f.a, f.c));
        const QString g = f.a->group();
        QVERIFY2(!g.isEmpty(), "group id was cleared by the link");
        QCOMPARE(f.ws.groupMembers(g).size(), 3);
        QVERIFY2(f.b->group() == g,
                 "the pre-existing member was orphaned out of the group");
        QCOMPARE(f.c->group(), g);
    }

    // And the reverse asymmetry: linking an ungrouped leaf to a grouped
    // target must adopt the TARGET's id, per workspace.md:123.
    void linkingUngroupedToGroupedAdoptsTargetsId() {
        Fixture f;
        f.build();
        f.a->setGroup(QStringLiteral("target-group"));
        QVERIFY(f.ws.linkLeaves(f.c, f.a));
        QCOMPARE(f.c->group(), QStringLiteral("target-group"));
    }

    void linkRejectsNullAndSelf() {
        Fixture f;
        f.build();
        QVERIFY(!f.ws.linkLeaves(nullptr, f.a));
        QVERIFY(!f.ws.linkLeaves(f.a, nullptr));
        QVERIFY(!f.ws.linkLeaves(f.a, f.a));
        QVERIFY2(!f.a->group().isEmpty() == false, "self-link must not assign a group");
    }

    // ---- linkLeaves: pin propagation (workspace.md:392) ----------------

    // Joining a pinned group forces the new member pinned. This is the one
    // place the reference is explicit, and getting it backwards silently
    // un-pins a pinned tab on the next interaction.
    void joiningAPinnedGroupForcesPinned() {
        Fixture f;
        f.build();
        f.a->setPinned(true);
        QVERIFY(f.a->pinned());
        QVERIFY(!f.b->pinned());

        QVERIFY(f.ws.linkLeaves(f.b, f.a));
        QVERIFY2(f.b->pinned(), "joining a pinned leaf did not force pinned");
    }

    // The mirror case: an unpinned member must not become pinned just because
    // it linked. Guards against over-eager propagation.
    void linkingUnpinnedLeavesLeavesThemUnpinned() {
        Fixture f;
        f.build();
        QVERIFY(f.ws.linkLeaves(f.b, f.c));
        QVERIFY(!f.b->pinned());
        QVERIFY(!f.c->pinned());
    }

    // A third member joining an already-pinned group also gets pinned, and
    // the pin reaches every existing member of that group (not just the two
    // passed in) via propagatePinToGroup.
    void linkingIntoAnExistingPinnedGroupPropagatesToAll() {
        Fixture f;
        f.build();
        f.a->setGroup(QStringLiteral("g"));
        f.a->setPinned(true);
        f.ws.linkLeaves(f.a, f.b);          // group g, b pinned too

        // c joins a's group, where the only pinned member is a.
        QVERIFY(f.ws.linkLeaves(f.a, f.c));
        QVERIFY2(f.c->pinned(), "new member of a pinned group is not pinned");
        QCOMPARE(f.ws.groupMembers(QStringLiteral("g")).size(), 3);
        for (auto *m : f.ws.groupMembers(QStringLiteral("g")))
            QVERIFY2(m->pinned(), "an existing member of the group was left unpinned");
    }

    // ---- unlinkLeaf: no half-cleared groups ----------------------------

    // Unlinking must clear the group on EVERY member. Clearing only the
    // passed leaf would leave survivors pointing at a group id that no
    // longer includes them — the classic half-cleared group.
    void unlinkClearsEveryMemberNotJustTheCaller() {
        Fixture f;
        f.build();
        QVERIFY(f.ws.linkLeaves(f.b, f.c));
        f.ws.linkLeaves(f.a, f.b);          // all three now in one group
        QCOMPARE(f.ws.groupMembers(f.a->group()).size(), 3);

        f.ws.unlinkLeaf(f.b);
        for (auto *leaf : {f.a, f.b, f.c}) {
            QVERIFY2(leaf->group().isEmpty(),
                     "a member was left pointing at a group it is no longer in");
        }
    }

    void unlinkIsSafeForNullAndUngrouped() {
        Fixture f;
        f.build();
        f.ws.unlinkLeaf(nullptr);            // must not crash
        f.ws.unlinkLeaf(f.a);                // a is ungrouped -> no-op
        QVERIFY(f.a->group().isEmpty());
        QCOMPARE(f.ws.leafCountInGroup(f.a), 3);   // tab grouping untouched
    }

    // ---- isLinkedPane ---------------------------------------------------

    // "Linked" means sharing a group with at least one OTHER leaf — a leaf
    // alone in a group is not a linked pane. A naive `!group().isEmpty()`
    // would report every grouped leaf as linked.
    void isLinkedPaneNeedsMoreThanOneMember() {
        Fixture f;
        f.build();
        f.a->setGroup(QStringLiteral("solo"));
        QVERIFY2(!f.ws.isLinkedPane(f.a), "a lone grouped leaf reported as linked");

        f.ws.linkLeaves(f.a, f.b);
        QVERIFY(f.ws.isLinkedPane(f.a));
        QVERIFY(f.ws.isLinkedPane(f.b));
        QVERIFY(!f.ws.isLinkedPane(f.c));
        QVERIFY(!f.ws.isLinkedPane(nullptr));
    }

    // ---- previousActiveLeaf: the "active pane" target -------------------

    // Obsidian's "Link with active pane" targets the previously-focused leaf.
    // Workspace only just gained that notion, so pin its behaviour: it
    // tracks the prior leaf and must not hand back the current one.
    void previousActiveLeafTracksThePriorLeaf() {
        Fixture f;
        f.build();
        ws_setup_layout_ready(f.ws);
        QCOMPARE(f.ws.previousActiveLeaf(), nullptr);

        f.ws.setActiveLeaf(f.a);
        QCOMPARE(f.ws.previousActiveLeaf(), nullptr);
        f.ws.setActiveLeaf(f.b);
        QCOMPARE(f.ws.previousActiveLeaf(), f.a);
        f.ws.setActiveLeaf(f.c);
        QCOMPARE(f.ws.previousActiveLeaf(), f.b);
    }

    // Re-focusing the SAME leaf is not a change (setActiveLeaf early-returns),
    // so previousActiveLeaf must not become the leaf itself — otherwise
    // "link with active pane" would enable against a self-link target.
    void previousActiveLeafNeverReturnsTheCurrentLeaf() {
        Fixture f;
        f.build();
        ws_setup_layout_ready(f.ws);
        f.ws.setActiveLeaf(f.a);
        f.ws.setActiveLeaf(f.b);
        QVERIFY(f.ws.previousActiveLeaf() != f.ws.activeLeaf());
    }

    // ---- tabSiblings: the tab-group membership answer -------------------

    // Go to Tab N resolves the target through this list, so it has to be tab
    // ordered and include the leaf itself. It used to be built by pairing
    // `tabGroupIdOf()` with `groupMembers()`, which live in two different id
    // spaces (the serializer's stacked key vs the linked-pane id) and so
    // always came back empty — every tab jump returned early and moved nothing
    // (found live 2026-10-18). A `groupMembers(leaf->group())`-shaped answer
    // would be empty here, not reordered, which is what makes this falsifiable.
    void tabSiblingsIsTabOrderedAndIncludesSelf() {
        Fixture f;
        f.build();
        const auto members = f.ws.tabSiblings(f.b);
        QCOMPARE(members.size(), 3);
        QCOMPARE(members.at(0), f.a);
        QCOMPARE(members.at(1), f.b);
        QCOMPARE(members.at(2), f.c);
        QCOMPARE(members.indexOf(f.b), f.ws.leafIndexInGroup(f.b));
        QCOMPARE(members.size(), f.ws.leafCountInGroup(f.a));
    }

    void tabSiblingsIsEmptyForANullLeaf() {
        Fixture f;
        f.build();
        QVERIFY(f.ws.tabSiblings(nullptr).isEmpty());
    }

    // The order follows the tab bar, not creation: after moving c left the
    // list must report the new order, or "Go to Tab 2" would select the wrong
    // tab. Pinned with an actual move rather than a fresh fixture on purpose.
    void tabSiblingsFollowsATabMove() {
        Fixture f;
        f.build();
        QVERIFY(f.ws.moveLeafWithinGroup(f.c, -1));
        const auto members = f.ws.tabSiblings(f.a);
        QCOMPARE(members.at(0), f.a);
        QCOMPARE(members.at(1), f.c);
        QCOMPARE(members.at(2), f.b);
    }

    // ---- tab-group id: the serializer's stacked-state key ---------------

    // `tabGroupIdOf` is what the serializer keys per-group stacked state on, so
    // a leaf sitting on an empty id is a group whose stacked flag cannot be
    // written back (the write path reads the pane's first leaf). Every leaf
    // that gets docked must therefore leave with a usable id, and leaves tabbed
    // into one pane must agree on it — a pane whose tabs disagree keys two
    // different stacked flags and loses one of them.
    void tabbedLeavesShareANonEmptyStackedKey() {
        Fixture f;
        f.build();
        const QString idA = f.ws.tabGroupIdOf(f.a);
        QVERIFY2(!idA.isEmpty(), "a docked leaf has no tab-group id to key on");
        QCOMPARE(f.ws.tabGroupIdOf(f.b), idA);
        QCOMPARE(f.ws.tabGroupIdOf(f.c), idA);

        // And the key round-trips as stacked state, which is its whole job.
        QVERIFY(!f.ws.isTabGroupStacked(idA));
        f.ws.setTabGroupStacked(idA, true);
        QVERIFY(f.ws.isTabGroupStacked(idA));
    }

    // A leaf created without placement (the serializer's path, which assigns
    // dock group + id explicitly afterwards) must not hand its *hole* on to a
    // leaf later tabbed into it: inheriting "" would leave both on an id no
    // stacked flag can be keyed to.
    void tabbingIntoAPlacelessLeafMintsAKeyRatherThanInheritingTheHole() {
        Fixture f;
        auto *anchor = f.ws.createLeafInGroupOf(nullptr);
        QVERIFY(anchor);
        auto *second = f.ws.createLeafInGroupOf(anchor);
        QVERIFY(second);
        QVERIFY2(!f.ws.tabGroupIdOf(anchor).isEmpty(),
                 "the anchor ended up with no tab-group id");
        QCOMPARE(f.ws.tabGroupIdOf(second), f.ws.tabGroupIdOf(anchor));
    }

private:
    // setActiveLeaf is gated on layout-ready, which tests must flip
    // explicitly or the call silently does nothing.
    static void ws_setup_layout_ready(Workspace &ws) { ws.setLayoutReady(true); }
};

QTEST_MAIN(TestWorkspaceLinkedPane)
#include "tst_workspace_linked_pane.moc"