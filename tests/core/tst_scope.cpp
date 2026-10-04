// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Scope — hierarchical key-handler stack (Cluster C Phase 2).
// Spec: docs/obsidian-audit/domains/core.md §1 (Scope) + domains/platform.md (Keymap).
//
// Obsidian contract:
//   - register(modifiers, key, fn) → KeymapEventHandler
//   - unregister(handler)
//   - handleKey(evt) walks child-first → parent, first hit wins
//   - child binding masks parent binding even when no-op
//
// Our payload is a callable taking the QKeyEvent* and returning
// true to consume, false to fall through to the parent.

#include <QTest>
#include <QKeyEvent>
#include <QObject>

#include "corbomite/core/Scope.h"

using Corbomite::Scope;
using Corbomite::KeyBinding;

namespace {

QKeyEvent makeKey(int key, Qt::KeyboardModifiers mods)
{
    return QKeyEvent(QEvent::KeyPress, key, mods);
}

} // namespace

class TestScope : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testEmptyScopeDoesNotHandle()
    {
        Scope s;
        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(!s.handleKey(&e));
    }

    void testRegisteredBindingHandles()
    {
        Scope s;
        int fired = 0;
        s.registerBinding(Qt::NoModifier, Qt::Key_A,
                          [&](QKeyEvent *) { ++fired; return true; });
        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(s.handleKey(&e));
        QCOMPARE(fired, 1);
    }

    void testModifierMatchingIsStrict()
    {
        Scope s;
        int fired = 0;
        s.registerBinding(Qt::ControlModifier, Qt::Key_S,
                          [&](QKeyEvent *) { ++fired; return true; });
        // Wrong modifier set — should not fire.
        auto e1 = makeKey(Qt::Key_S, Qt::NoModifier);
        QVERIFY(!s.handleKey(&e1));
        // Right modifier — fires.
        auto e2 = makeKey(Qt::Key_S, Qt::ControlModifier);
        QVERIFY(s.handleKey(&e2));
        QCOMPARE(fired, 1);
    }

    void testCallbackReturningFalseFallsThrough()
    {
        Scope parent;
        int parentFired = 0;
        parent.registerBinding(Qt::NoModifier, Qt::Key_A,
                               [&](QKeyEvent *) { ++parentFired; return true; });

        Scope child(&parent);
        int childFired = 0;
        child.registerBinding(Qt::NoModifier, Qt::Key_A,
                              [&](QKeyEvent *) { ++childFired; return false; });

        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(child.handleKey(&e));
        QCOMPARE(childFired, 1);
        QCOMPARE(parentFired, 1);
    }

    void testChildMasksParentWhenReturningTrue()
    {
        Scope parent;
        int parentFired = 0;
        parent.registerBinding(Qt::NoModifier, Qt::Key_A,
                               [&](QKeyEvent *) { ++parentFired; return true; });

        Scope child(&parent);
        int childFired = 0;
        child.registerBinding(Qt::NoModifier, Qt::Key_A,
                              [&](QKeyEvent *) { ++childFired; return true; });

        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(child.handleKey(&e));
        QCOMPARE(childFired, 1);
        QCOMPARE(parentFired, 0);
    }

    // Preserve Obsidian quirk: a child binding for Ctrl+S masks the parent
    // even if the child's callback is a no-op that returns true.
    void testChildNoOpStillMasksParent()
    {
        Scope parent;
        int parentFired = 0;
        parent.registerBinding(Qt::ControlModifier, Qt::Key_S,
                               [&](QKeyEvent *) { ++parentFired; return true; });

        Scope child(&parent);
        child.registerBinding(Qt::ControlModifier, Qt::Key_S,
                              [](QKeyEvent *) { return true; });

        auto e = makeKey(Qt::Key_S, Qt::ControlModifier);
        QVERIFY(child.handleKey(&e));
        QCOMPARE(parentFired, 0);
    }

    void testUnregisterRemovesBinding()
    {
        Scope s;
        int fired = 0;
        auto handle = s.registerBinding(Qt::NoModifier, Qt::Key_A,
                                        [&](QKeyEvent *) { ++fired; return true; });
        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(s.handleKey(&e));
        QCOMPARE(fired, 1);

        s.unregister(handle);
        QVERIFY(!s.handleKey(&e));
        QCOMPARE(fired, 1);
    }

    void testThreeLevelChainWalksToRoot()
    {
        Scope root;
        int rootFired = 0;
        root.registerBinding(Qt::NoModifier, Qt::Key_A,
                             [&](QKeyEvent *) { ++rootFired; return true; });

        Scope mid(&root);
        Scope leaf(&mid);

        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(leaf.handleKey(&e));
        QCOMPARE(rootFired, 1);
    }

    void testMultipleBindingsOnSameKeyFireInOrderUntilConsumed()
    {
        Scope s;
        QStringList log;
        s.registerBinding(Qt::NoModifier, Qt::Key_A,
                          [&](QKeyEvent *) { log << QStringLiteral("A"); return false; });
        s.registerBinding(Qt::NoModifier, Qt::Key_A,
                          [&](QKeyEvent *) { log << QStringLiteral("B"); return true; });
        s.registerBinding(Qt::NoModifier, Qt::Key_A,
                          [&](QKeyEvent *) { log << QStringLiteral("C"); return true; });

        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(s.handleKey(&e));
        // A returned false (fell through), B consumed, C never ran.
        QCOMPARE(log, (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
    }

    // --- catch-all (Obsidian's register(null, null, …)) --------------------

    void testCatchAllSeesKeysWithNoExactBinding()
    {
        Scope s;
        QStringList seen;
        s.registerCatchAll([&](QKeyEvent *e) { seen << QString::number(e->key()); return false; });

        auto unmatched = makeKey(Qt::Key_Q, Qt::ControlModifier);
        s.handleKey(&unmatched);
        // A key the scope does claim is not offered to the catch-all.
        s.registerBinding(Qt::NoModifier, Qt::Key_Escape,
                          [](QKeyEvent *) { return true; });
        auto claimed = makeKey(Qt::Key_Escape, Qt::NoModifier);
        QVERIFY(s.handleKey(&claimed));
        QCOMPARE(seen, QStringList{QString::number(Qt::Key_Q)});
    }

    // Registration order must not decide: a catch-all registered first still
    // loses to a concrete binding. That is what lets a modal's Esc and a
    // whole command table share one scope.
    void testCatchAllNeverPreemptsAConcreteBinding()
    {
        Scope s;
        bool catchAllRan = false;
        bool concreteRan = false;
        s.registerCatchAll([&](QKeyEvent *) { catchAllRan = true; return true; });
        s.registerBinding(Qt::ControlModifier, Qt::Key_S,
                          [&](QKeyEvent *) { concreteRan = true; return true; });

        auto e = makeKey(Qt::Key_S, Qt::ControlModifier);
        QVERIFY(s.handleKey(&e));
        QVERIFY(concreteRan);
        QVERIFY(!catchAllRan);
    }

    void testUnregisterRemovesCatchAll()
    {
        Scope s;
        int fired = 0;
        auto handle = s.registerCatchAll(
            [&](QKeyEvent *) { ++fired; return true; });
        auto e = makeKey(Qt::Key_Z, Qt::NoModifier);
        QVERIFY(s.handleKey(&e));
        QCOMPARE(fired, 1);
        s.unregister(handle);
        QVERIFY(!s.handleKey(&e));
        QCOMPARE(fired, 1);
    }

    // Obsidian's rule is the opposite for a concrete binding that declines:
    // the walk stops without consuming. Ours falls through to the parent (see
    // Scope's class comment) — pinned here so the divergence stays deliberate.
    void testCatchAllIsAfterSiblingsButBeforeParent()
    {
        Scope parent;
        int parentFired = 0;
        parent.registerBinding(Qt::NoModifier, Qt::Key_A,
                               [&](QKeyEvent *) { ++parentFired; return true; });

        Scope child(&parent);
        int catchAllFired = 0;
        child.registerCatchAll(
            [&](QKeyEvent *) { ++catchAllFired; return true; });

        auto e = makeKey(Qt::Key_A, Qt::NoModifier);
        QVERIFY(child.handleKey(&e));
        QCOMPARE(catchAllFired, 1);
        QCOMPARE(parentFired, 0);
    }
};

QTEST_MAIN(TestScope)
#include "tst_scope.moc"
