// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::ScopeManager — the QApplication-level dispatcher
// that walks the LIFO scope stack on key events (Cluster C Phase 2).

#include <QTest>
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QKeyEvent>

#include "corbomite/core/Scope.h"
#include "corbomite/core/ScopeManager.h"

using Corbomite::Scope;
using Corbomite::ScopeManager;

namespace {

QKeyEvent makeKey(int key, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    return QKeyEvent(QEvent::KeyPress, key, mods);
}

} // namespace

class TestScopeManager : public QObject {
    Q_OBJECT

private:
    ScopeManager *mgr() { return ScopeManager::instance(); }

private Q_SLOTS:
    void initTestCase()
    {
        // Clear any stale state between test processes (singleton).
        while (mgr()->stackDepth() > 0) mgr()->popScope();
        mgr()->setRootScope(nullptr);
    }

    void cleanup()
    {
        while (mgr()->stackDepth() > 0) mgr()->popScope();
        mgr()->setRootScope(nullptr);
        mgr()->setBypassPredicate({}); // restore default
        mgr()->setRootGate({});
    }

    // Modal Esc masks editor Esc while open; editor Esc wins when closed.
    void testModalScopeWinsOverEditorScope()
    {
        Scope editorScope;
        int editorFired = 0;
        editorScope.registerBinding(Qt::NoModifier, Qt::Key_Escape,
            [&](QKeyEvent *) { ++editorFired; return true; });

        Scope modalScope;
        int modalFired = 0;
        modalScope.registerBinding(Qt::NoModifier, Qt::Key_Escape,
            [&](QKeyEvent *) { ++modalFired; return true; });

        mgr()->pushScope(&editorScope);

        // Editor alone: editor handles Esc.
        auto e1 = makeKey(Qt::Key_Escape);
        QVERIFY(mgr()->dispatchKey(&e1));
        QCOMPARE(editorFired, 1);
        QCOMPARE(modalFired, 0);

        // Modal opens and pushes its scope.
        mgr()->pushScope(&modalScope);
        auto e2 = makeKey(Qt::Key_Escape);
        QVERIFY(mgr()->dispatchKey(&e2));
        QCOMPARE(modalFired, 1);
        QCOMPARE(editorFired, 1); // editor did NOT fire

        // Modal closes.
        mgr()->popScope();
        auto e3 = makeKey(Qt::Key_Escape);
        QVERIFY(mgr()->dispatchKey(&e3));
        QCOMPARE(modalFired, 1);
        QCOMPARE(editorFired, 2);
    }

    void testEmptyStackReturnsFalse()
    {
        auto e = makeKey(Qt::Key_A);
        QVERIFY(!mgr()->dispatchKey(&e));
    }

    // Pins the divergence documented at ScopeManager::dispatchKey and in its
    // class comment: Obsidian's per-window scope stack means a parentless
    // `Modal` scope cuts the chain (0817-Yb.js:630 + :709, addendum §E),
    // Corbomite's flat stack does not. This is what makes the root slot — the
    // command table's position — reachable at all while a modal scope is up.
    void testParentlessScopeDoesNotCutScopesBelowIt()
    {
        Scope rootish;
        int rootFired = 0;
        rootish.registerBinding(Qt::NoModifier, Qt::Key_B,
            [&](QKeyEvent *) { ++rootFired; return true; });

        Scope parentlessModal; // Scope() default ctor: m_parent == nullptr
        QVERIFY(!parentlessModal.parent());
        mgr()->pushScope(&rootish);
        mgr()->pushScope(&parentlessModal);

        auto e = makeKey(Qt::Key_B);
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(rootFired, 1);
        QCOMPARE(mgr()->stackDepth(), 2);
    }

    void testUnmatchedKeyReturnsFalse()
    {
        Scope s;
        s.registerBinding(Qt::NoModifier, Qt::Key_A,
                          [](QKeyEvent *) { return true; });
        mgr()->pushScope(&s);
        auto e = makeKey(Qt::Key_B);
        QVERIFY(!mgr()->dispatchKey(&e));
    }

    void testRemoveScopeFromMiddle()
    {
        Scope a, b, c;
        int aFired = 0, bFired = 0, cFired = 0;
        a.registerBinding(Qt::NoModifier, Qt::Key_X,
            [&](QKeyEvent *) { ++aFired; return true; });
        b.registerBinding(Qt::NoModifier, Qt::Key_X,
            [&](QKeyEvent *) { ++bFired; return true; });
        c.registerBinding(Qt::NoModifier, Qt::Key_X,
            [&](QKeyEvent *) { ++cFired; return true; });

        mgr()->pushScope(&a);
        mgr()->pushScope(&b);
        mgr()->pushScope(&c);
        QCOMPARE(mgr()->stackDepth(), 3);

        // Remove middle scope b.
        mgr()->removeScope(&b);
        QCOMPARE(mgr()->stackDepth(), 2);

        auto e = makeKey(Qt::Key_X);
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(cFired, 1);
        QCOMPARE(bFired, 0);
        QCOMPARE(aFired, 0);
    }

    void testBypassPredicateSkipsDispatch()
    {
        Scope s;
        int fired = 0;
        s.registerBinding(Qt::NoModifier, Qt::Key_A,
            [&](QKeyEvent *) { ++fired; return true; });
        mgr()->pushScope(&s);

        mgr()->setBypassPredicate([](QWidget *) { return true; });
        QLineEdit edit;
        auto e = makeKey(Qt::Key_A);
        QVERIFY(!mgr()->dispatchKey(&e, &edit));
        QCOMPARE(fired, 0);

        mgr()->setBypassPredicate([](QWidget *) { return false; });
        QVERIFY(mgr()->dispatchKey(&e, &edit));
        QCOMPARE(fired, 1);
    }

    // Default bypass predicate identifies QLineEdit as a bypass target.
    void testDefaultBypassForQLineEdit()
    {
        Scope s;
        int fired = 0;
        s.registerBinding(Qt::NoModifier, Qt::Key_A,
            [&](QKeyEvent *) { ++fired; return true; });
        mgr()->pushScope(&s);
        // Default predicate installed at construction.

        QLineEdit edit;
        auto e = makeKey(Qt::Key_A);
        QVERIFY(!mgr()->dispatchKey(&e, &edit));
        QCOMPARE(fired, 0);

        // With no focus widget, dispatch goes through.
        QVERIFY(mgr()->dispatchKey(&e, nullptr));
        QCOMPARE(fired, 1);
    }

    // --- the root slot ------------------------------------------------------

    void testRootScopeIsConsultedWithAnEmptyStack()
    {
        Scope root;
        int fired = 0;
        root.registerBinding(Qt::ControlModifier, Qt::Key_J,
                             [&](QKeyEvent *) { ++fired; return true; });
        mgr()->setRootScope(&root);
        QCOMPARE(mgr()->rootScope(), &root);
        // An empty pushed stack used to mean "nothing to dispatch"; the command
        // table lives in the root slot, so that would disable every override.
        QCOMPARE(mgr()->stackDepth(), 0);
        auto e = makeKey(Qt::Key_J, Qt::ControlModifier);
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(fired, 1);
    }

    void testPushedScopeWinsOverTheRootAndDecliningFallsThrough()
    {
        int rootFired = 0;
        Scope root;
        root.registerBinding(Qt::ControlModifier, Qt::Key_J,
                             [&](QKeyEvent *) { ++rootFired; return true; });
        mgr()->setRootScope(&root);

        Scope pushed;
        bool claim = false;
        pushed.registerBinding(Qt::ControlModifier, Qt::Key_J,
                               [&](QKeyEvent *) { return claim; });
        mgr()->pushScope(&pushed);

        auto e = makeKey(Qt::Key_J, Qt::ControlModifier);
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(rootFired, 1);

        claim = true;
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(rootFired, 1);
    }

    void testRootGateCanCloseTheRootSlot()
    {
        Scope root;
        int fired = 0;
        root.registerBinding(Qt::ControlModifier, Qt::Key_J,
                             [&](QKeyEvent *) { ++fired; return true; });
        mgr()->setRootScope(&root);
        mgr()->setRootGate([] { return false; });

        auto e = makeKey(Qt::Key_J, Qt::ControlModifier);
        QVERIFY(!mgr()->dispatchKey(&e));
        QCOMPARE(fired, 0);

        mgr()->setRootGate([] { return true; });
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(fired, 1);
    }

    // The default gate is Qt modality: a modal dialog's key presses must not
    // fire the main window's commands (addendum §E).
    void testDefaultRootGateClosesWhileAModalIsUp()
    {
        Scope root;
        int fired = 0;
        root.registerBinding(Qt::ControlModifier, Qt::Key_J,
                             [&](QKeyEvent *) { ++fired; return true; });
        mgr()->setRootScope(&root);

        auto e = makeKey(Qt::Key_J, Qt::ControlModifier);
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(fired, 1);

        auto *dialog = new QDialog;
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->show();
        QVERIFY(qApp->activeModalWidget());
        QVERIFY(!mgr()->dispatchKey(&e));
        QCOMPARE(fired, 1);
        dialog->close();
        delete dialog;
        QVERIFY(mgr()->dispatchKey(&e));
        QCOMPARE(fired, 2);
    }

    // The focus gate is for pushed scopes. If it also silenced the root, every
    // global shortcut would die the moment a search box took focus.
    void testFocusGateDoesNotSilenceTheRoot()
    {
        Scope root;
        int fired = 0;
        root.registerBinding(Qt::ControlModifier, Qt::Key_U,
                             [&](QKeyEvent *) { ++fired; return true; });
        mgr()->setRootScope(&root);

        Scope pushed;
        int pushedFired = 0;
        pushed.registerBinding(Qt::ControlModifier, Qt::Key_U,
                               [&](QKeyEvent *) { ++pushedFired; return true; });
        mgr()->pushScope(&pushed);

        QLineEdit edit;
        auto e = makeKey(Qt::Key_U, Qt::ControlModifier);
        QVERIFY(mgr()->dispatchKey(&e, &edit));
        QCOMPARE(pushedFired, 0);
        QCOMPARE(fired, 1);
    }
};

QTEST_MAIN(TestScopeManager)
#include "tst_scopemanager.moc"
