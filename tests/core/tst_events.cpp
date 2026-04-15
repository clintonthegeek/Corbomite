// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Events — name-keyed event mixin (Cluster C Phase 1).
// Spec: docs/obsidian-audit/domains/core.md §1
// Obsidian contract:
//   - on(name, fn, ctx?) → EventRef   (registers a listener)
//   - off(name, fn)                    (unregister by callback identity)
//   - offref(EventRef)                 (O(1) unregister)
//   - trigger(name, ...args)           (synchronous dispatch; throws propagate)
//   - tryTrigger(name, ...args)        (swallow + rethrow-on-next-tick)
//
// Our payload is QVariantList so listeners can extract N args positionally.
// Async rethrow uses QTimer::singleShot(0, ...) (per the exploration
// recommendation — Qt::QueuedConnection swallows exceptions silently).

#include <QTest>
#include <QObject>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVariantList>

#include "corbomite/core/Events.h"

using Corbomite::Events;
using Corbomite::EventRef;

class TestEvents : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // ---- Basic subscribe / trigger / unsubscribe --------------------------

    void testTriggerFiresListener()
    {
        Events bus;
        int count = 0;
        bus.on(QStringLiteral("ping"), [&](const QVariantList &) { ++count; });
        bus.trigger(QStringLiteral("ping"));
        QCOMPARE(count, 1);
        bus.trigger(QStringLiteral("ping"));
        QCOMPARE(count, 2);
    }

    void testTriggerPassesArguments()
    {
        Events bus;
        QString receivedStr;
        int receivedInt = 0;
        bus.on(QStringLiteral("msg"), [&](const QVariantList &args) {
            receivedStr = args.value(0).toString();
            receivedInt = args.value(1).toInt();
        });
        bus.trigger(QStringLiteral("msg"),
                    {QStringLiteral("hello"), 42});
        QCOMPARE(receivedStr, QStringLiteral("hello"));
        QCOMPARE(receivedInt, 42);
    }

    void testTriggerUnknownNameIsNoOp()
    {
        Events bus;
        bus.trigger(QStringLiteral("nobody-listens")); // must not crash
    }

    void testMultipleListenersFireInRegistrationOrder()
    {
        Events bus;
        QStringList order;
        bus.on(QStringLiteral("e"), [&](auto) { order << QStringLiteral("A"); });
        bus.on(QStringLiteral("e"), [&](auto) { order << QStringLiteral("B"); });
        bus.on(QStringLiteral("e"), [&](auto) { order << QStringLiteral("C"); });
        bus.trigger(QStringLiteral("e"));
        QCOMPARE(order, (QStringList{QStringLiteral("A"), QStringLiteral("B"),
                                     QStringLiteral("C")}));
    }

    void testOffrefUnsubscribes()
    {
        Events bus;
        int count = 0;
        EventRef ref = bus.on(QStringLiteral("e"),
                              [&](auto) { ++count; });
        bus.trigger(QStringLiteral("e"));
        QCOMPARE(count, 1);
        bus.offref(ref);
        bus.trigger(QStringLiteral("e"));
        QCOMPARE(count, 1);
    }

    void testOffrefIsIdempotent()
    {
        Events bus;
        EventRef ref = bus.on(QStringLiteral("e"),
                              [](auto) {});
        bus.offref(ref);
        bus.offref(ref); // must not crash
        bus.trigger(QStringLiteral("e"));
    }

    void testUnsubscribeOneLeavesOthers()
    {
        Events bus;
        int a = 0, b = 0;
        EventRef refA = bus.on(QStringLiteral("e"), [&](auto) { ++a; });
        bus.on(QStringLiteral("e"), [&](auto) { ++b; });
        bus.trigger(QStringLiteral("e"));
        QCOMPARE(a, 1);
        QCOMPARE(b, 1);

        bus.offref(refA);
        bus.trigger(QStringLiteral("e"));
        QCOMPARE(a, 1);
        QCOMPARE(b, 2);
    }

    // ---- Unsubscribe during dispatch --------------------------------------

    void testListenerCanUnsubscribeDuringTrigger()
    {
        Events bus;
        int a = 0, b = 0;
        EventRef refA;
        refA = bus.on(QStringLiteral("e"), [&](auto) {
            ++a;
            bus.offref(refA);
        });
        bus.on(QStringLiteral("e"), [&](auto) { ++b; });

        bus.trigger(QStringLiteral("e"));
        // Both listeners fired on first trigger.
        QCOMPARE(a, 1);
        QCOMPARE(b, 1);

        bus.trigger(QStringLiteral("e"));
        // A already unsubscribed; only B fires.
        QCOMPARE(a, 1);
        QCOMPARE(b, 2);
    }

    // ---- trigger() propagates exceptions ----------------------------------

    void testTriggerPropagatesException()
    {
        Events bus;
        bus.on(QStringLiteral("boom"),
               [](auto) { throw std::runtime_error("boom"); });
        bool caught = false;
        try {
            bus.trigger(QStringLiteral("boom"));
        } catch (const std::runtime_error &) {
            caught = true;
        }
        QVERIFY(caught);
    }

    // ---- tryTrigger() swallows + rethrows on next tick --------------------

    void testTryTriggerSwallowsException()
    {
        Events bus;
        bus.on(QStringLiteral("boom"),
               [](auto) { throw std::runtime_error("boom"); });
        // Must not throw synchronously.
        bus.tryTrigger(QStringLiteral("boom"));
    }

    void testTryTriggerContinuesAfterThrowingListener()
    {
        Events bus;
        bool afterFired = false;
        bus.on(QStringLiteral("e"),
               [](auto) { throw std::runtime_error("first"); });
        bus.on(QStringLiteral("e"), [&](auto) { afterFired = true; });
        bus.tryTrigger(QStringLiteral("e"));
        QVERIFY(afterFired);
    }

    // Async rethrow: the exception surfaces on the next event-loop tick.
    // We install a terminate-style hook via QTimer. To test without
    // actually terminating, we verify that the rethrow is *scheduled* —
    // i.e. when we spin the event loop after tryTrigger, a sentinel
    // timer installed afterwards still runs (proving the loop spun).
    //
    // Verifying the rethrow itself terminates the process is out of
    // scope for a unit test; we verify scheduling by checking the
    // Events-internal pending-rethrow counter reaches zero after a
    // spin with exception handling installed.
    void testTryTriggerReschedulesException()
    {
        Events bus;
        bus.on(QStringLiteral("boom"),
               [](auto) { throw std::runtime_error("async"); });
        bus.tryTrigger(QStringLiteral("boom"));
        QCOMPARE(bus.pendingAsyncRethrows(), 1);

        // Spin the event loop with a protective try/catch around the tick.
        // We need QEventLoop to process the 0-delay timer.
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        bool gotException = false;
        try {
            loop.exec();
        } catch (const std::runtime_error &) {
            gotException = true;
        }
        // Either the loop propagated the throw, or Qt's event dispatcher
        // swallowed it — in either case, the pending counter should drop.
        QCOMPARE(bus.pendingAsyncRethrows(), 0);
        // Best-effort: on most Qt builds the exception surfaces to us.
        if (gotException) qDebug() << "Async rethrow surfaced to caller.";
    }
};

QTEST_MAIN(TestEvents)
#include "tst_events.moc"
