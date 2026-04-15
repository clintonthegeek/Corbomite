// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QVariantList>

#include <exception>
#include <functional>
#include <memory>

namespace Corbomite {

class Events;

/// Opaque handle returned by `Events::on()`. Pass to `offref()` to remove
/// the listener in amortized O(1). Copyable/assignable; the only consumer
/// is `offref()` which treats a stale ref as a no-op.
class EventRef
{
public:
    EventRef() = default;

private:
    friend class Events;
    struct Node; // defined in Events.cpp
    std::weak_ptr<Node> m_node;
    explicit EventRef(const std::shared_ptr<Node> &n) : m_node(n) {}
};

/// Name-keyed event bus, modeled on Obsidian's `Events` mixin
/// (see `docs/obsidian-audit/domains/core.md §1`).
///
/// Usage: inherit alongside QObject — or embed as a member — and expose
/// `on/off/trigger` on the owning type as needed. Kept as a plain base
/// (not CRTP) per the Cluster C exploration: Qt MOC does not process
/// templates, and a non-virtual plain base composes cleanly with any
/// QObject subclass without complicating MOC.
///
/// Semantics:
///   - `on(name, fn)` returns an `EventRef` and appends to the listener
///     list for that event name.
///   - `off(name, fn)` removes by callback identity. Not typically
///     useful for lambdas; prefer `offref()`.
///   - `offref(ref)` unregisters one listener; idempotent.
///   - `trigger(name, args)` fires listeners synchronously, in
///     registration order. Exceptions propagate.
///   - `tryTrigger(name, args)` runs each listener in a try/catch;
///     the first exception is scheduled to rethrow on the next event-loop
///     tick via `QTimer::singleShot(0, ...)` (chosen over
///     `Qt::QueuedConnection` because queued invocations swallow
///     exceptions silently — see Cluster C exploration).
///
/// Listeners can safely `offref` themselves during dispatch; the copy-
/// before-iterate invariant ensures in-flight dispatch isn't disturbed.
class Events
{
public:
    using Listener = std::function<void(const QVariantList &)>;

    Events();
    virtual ~Events();

    Events(const Events &) = delete;
    Events &operator=(const Events &) = delete;

    /// Subscribe. Returns an EventRef usable with `offref`.
    EventRef on(const QString &name, Listener fn);

    /// Unsubscribe by EventRef. Idempotent (stale refs are no-ops).
    void offref(const EventRef &ref);

    /// Synchronous dispatch. Exceptions from listeners propagate to the
    /// caller. Listeners fire in registration order. Safe to mutate the
    /// listener list from inside a listener.
    void trigger(const QString &name,
                 const QVariantList &args = QVariantList{});

    /// Like `trigger`, but each listener's exceptions are caught and
    /// rescheduled to re-throw on the next event-loop tick (via
    /// `QTimer::singleShot(0, ...)`). Subsequent listeners still run.
    void tryTrigger(const QString &name,
                    const QVariantList &args = QVariantList{});

    /// Test hook: how many exceptions are pending async rethrow. Drops
    /// to zero after the next event-loop spin (minus any currently
    /// mid-rethrow).
    int pendingAsyncRethrows() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Corbomite
