// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Events.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QTimer>

#include <exception>
#include <memory>

namespace Corbomite {

// Node lives in a shared_ptr so that EventRef (weak_ptr) naturally
// expires when the listener is removed. Guarantees offref idempotence.
struct EventRef::Node
{
    QString name;
    Events::Listener fn;
    bool active = true;
};

struct Events::Impl
{
    QHash<QString, QList<std::shared_ptr<EventRef::Node>>> byName;
    QAtomicInt pendingRethrows{0};
};

Events::Events() : m_impl(std::make_unique<Impl>()) {}
Events::~Events() = default;

EventRef Events::on(const QString &name, Listener fn)
{
    auto node = std::make_shared<EventRef::Node>();
    node->name = name;
    node->fn = std::move(fn);
    m_impl->byName[name].append(node);
    return EventRef(node);
}

void Events::offref(const EventRef &ref)
{
    auto node = ref.m_node.lock();
    if (!node) return;
    node->active = false;
    auto it = m_impl->byName.find(node->name);
    if (it == m_impl->byName.end()) return;
    it->removeOne(node);
    if (it->isEmpty()) m_impl->byName.erase(it);
}

void Events::trigger(const QString &name, const QVariantList &args)
{
    auto it = m_impl->byName.find(name);
    if (it == m_impl->byName.end()) return;
    // Snapshot to tolerate offref during dispatch.
    const auto snapshot = *it;
    for (const auto &node : snapshot) {
        if (node && node->active && node->fn) {
            node->fn(args);
        }
    }
}

void Events::tryTrigger(const QString &name, const QVariantList &args)
{
    auto it = m_impl->byName.find(name);
    if (it == m_impl->byName.end()) return;
    const auto snapshot = *it;
    for (const auto &node : snapshot) {
        if (!node || !node->active || !node->fn) continue;
        try {
            node->fn(args);
        } catch (...) {
            std::exception_ptr eptr = std::current_exception();
            m_impl->pendingRethrows.fetchAndAddOrdered(1);
            QAtomicInt *counter = &m_impl->pendingRethrows;
            // QTimer::singleShot(0) schedules the rethrow on the next
            // event-loop tick. Chosen over Qt::QueuedConnection because
            // queued invocations catch & discard exceptions silently.
            QTimer::singleShot(0, [eptr, counter]() {
                counter->fetchAndSubOrdered(1);
                std::rethrow_exception(eptr);
            });
        }
    }
}

int Events::pendingAsyncRethrows() const
{
    return m_impl->pendingRethrows.loadAcquire();
}

} // namespace Corbomite
