// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Component — universal lifecycle base (Cluster C Phase 1).
// Spec: docs/obsidian-audit/domains/ui-bundle.md §1 (components/Component.js)
// Obsidian contract:
//   - load() → isLoaded = true, fires onload(); recursive: children loaded first
//   - unload() → isLoaded = false, fires onunload(); recursive: children unloaded LIFO
//   - addChild(child) takes ownership; auto-loaded if parent is loaded
//   - removeChild(child) unloads and removes
//   - registerInterval(id) schedules clearInterval on unload
//   - registerEvent(ref) schedules offref on unload
// Our adaptation for Qt: registerInterval takes milliseconds + callable, returns opaque id;
// registerQObjectConnection takes a QMetaObject::Connection that is disconnected on unload.

#include <QTest>
#include <QObject>
#include <QTimer>

#include "corbomite/core/Component.h"

using Corbomite::Component;

class TestComponent : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // ---- Core lifecycle ---------------------------------------------------

    void testInitiallyUnloaded()
    {
        Component c;
        QVERIFY(!c.isLoaded());
    }

    void testLoadFiresOnload()
    {
        struct Probe : Component {
            int loads = 0;
            void onload() override { ++loads; }
        };
        Probe p;
        p.load();
        QVERIFY(p.isLoaded());
        QCOMPARE(p.loads, 1);
    }

    void testDoubleLoadIsNoOp()
    {
        struct Probe : Component {
            int loads = 0;
            void onload() override { ++loads; }
        };
        Probe p;
        p.load();
        p.load();
        QCOMPARE(p.loads, 1);
    }

    void testUnloadFiresOnunload()
    {
        struct Probe : Component {
            int unloads = 0;
            void onunload() override { ++unloads; }
        };
        Probe p;
        p.load();
        p.unload();
        QVERIFY(!p.isLoaded());
        QCOMPARE(p.unloads, 1);
    }

    void testUnloadWithoutLoadIsNoOp()
    {
        struct Probe : Component {
            int unloads = 0;
            void onunload() override { ++unloads; }
        };
        Probe p;
        p.unload();
        QCOMPARE(p.unloads, 0);
    }

    // ---- Child management -------------------------------------------------

    void testAddChildTakesOwnership()
    {
        Component parent;
        auto *child = new Component();
        parent.addChild(child);
        QCOMPARE(parent.childCount(), 1);
    }

    void testAddChildToLoadedParentAutoLoadsChild()
    {
        struct Probe : Component {
            int loads = 0;
            void onload() override { ++loads; }
        };
        Component parent;
        parent.load();
        auto *child = new Probe();
        parent.addChild(child);
        QVERIFY(child->isLoaded());
        QCOMPARE(child->loads, 1);
    }

    void testAddChildToUnloadedParentLeavesChildUnloaded()
    {
        Component parent;
        auto *child = new Component();
        parent.addChild(child);
        QVERIFY(!child->isLoaded());
    }

    void testUnloadIsLifoAcrossChildren()
    {
        QStringList log;
        struct Probe : Component {
            QStringList *log;
            QString tag;
            Probe(QStringList *l, QString t) : log(l), tag(std::move(t)) {}
            void onunload() override { log->append(tag); }
        };
        Component parent;
        parent.addChild(new Probe(&log, QStringLiteral("A")));
        parent.addChild(new Probe(&log, QStringLiteral("B")));
        parent.addChild(new Probe(&log, QStringLiteral("C")));
        parent.load();
        parent.unload();
        // LIFO: last added unloaded first. Parent's own onunload runs after
        // children per Obsidian semantics (children cleaned first).
        QCOMPARE(log, (QStringList{QStringLiteral("C"), QStringLiteral("B"), QStringLiteral("A")}));
    }

    void testRemoveChildUnloadsAndDeletes()
    {
        struct Probe : Component {
            bool *destroyedFlag;
            int unloads = 0;
            explicit Probe(bool *f) : destroyedFlag(f) {}
            ~Probe() override { *destroyedFlag = true; }
            void onunload() override { ++unloads; }
        };
        Component parent;
        parent.load();
        bool destroyed = false;
        auto *child = new Probe(&destroyed);
        parent.addChild(child);
        parent.removeChild(child);
        QCOMPARE(parent.childCount(), 0);
        QVERIFY(destroyed);
    }

    // ---- registerInterval -------------------------------------------------

    void testRegisterIntervalFiresAndStopsOnUnload()
    {
        Component c;
        c.load();
        int tickCount = 0;
        c.registerInterval(20, [&]() { ++tickCount; });
        QTest::qWait(120);  // ~5 ticks expected
        QVERIFY(tickCount >= 3);
        const int atUnload = tickCount;
        c.unload();
        QTest::qWait(80);
        // After unload, the timer must not fire any more.
        QCOMPARE(tickCount, atUnload);
    }

    // ---- registerQObjectConnection ---------------------------------------

    void testRegisterQObjectConnectionDisconnectsOnUnload()
    {
        QObject emitter;
        auto *timer = new QTimer(&emitter);
        timer->setSingleShot(false);

        Component c;
        c.load();

        int received = 0;
        auto conn = QObject::connect(timer, &QTimer::timeout, [&]() { ++received; });
        c.registerQObjectConnection(conn);

        timer->start(20);
        QTest::qWait(80);
        QVERIFY(received >= 2);
        const int atUnload = received;

        c.unload();
        QTest::qWait(80);
        // Callback must not fire post-unload (connection disconnected).
        QCOMPARE(received, atUnload);

        timer->stop();
    }

    // ---- Destruction ------------------------------------------------------

    // C++ limitation vs Obsidian JS: virtual `onunload` on *self* cannot
    // be dispatched during ~Component() because the subclass vtable is
    // already gone. Users who need their own onunload to fire must call
    // unload() before destruction. However, children's onunload still
    // fires — their vtables are intact — and registered resources
    // (intervals, connections) are still cleaned on destruction.

    void testDestructionUnloadsChildrenWithOnunload()
    {
        QStringList log;
        struct ChildProbe : Component {
            QStringList *log;
            QString tag;
            ChildProbe(QStringList *l, QString t) : log(l), tag(std::move(t)) {}
            void onunload() override { log->append(tag); }
        };
        {
            Component parent;
            parent.addChild(new ChildProbe(&log, QStringLiteral("A")));
            parent.addChild(new ChildProbe(&log, QStringLiteral("B")));
            parent.load();
        } // ~parent here
        // Children unloaded LIFO during parent destruction.
        QCOMPARE(log, (QStringList{QStringLiteral("B"), QStringLiteral("A")}));
    }

    void testDestructionCleansIntervals()
    {
        int ticks = 0;
        {
            Component c;
            c.load();
            c.registerInterval(20, [&]() { ++ticks; });
            QTest::qWait(80);
            QVERIFY(ticks >= 2);
        } // destructor runs here
        const int atDestroy = ticks;
        QTest::qWait(80);
        QCOMPARE(ticks, atDestroy);
    }
};

QTEST_MAIN(TestComponent)
#include "tst_component.moc"
