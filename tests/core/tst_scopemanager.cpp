// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::ScopeManager — the QApplication-level dispatcher
// that walks the LIFO scope stack on key events (Cluster C Phase 2).

#include <QTest>
#include <QApplication>
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
    }

    void cleanup()
    {
        while (mgr()->stackDepth() > 0) mgr()->popScope();
        mgr()->setBypassPredicate({}); // restore default
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
};

QTEST_MAIN(TestScopeManager)
#include "tst_scopemanager.moc"
