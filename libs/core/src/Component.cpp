// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Component.h"

#include <QObject>
#include <QTimerEvent>

namespace Corbomite {

// Private QObject that hosts timer events. Per-Component; constructed lazily
// on first registerInterval().
class Component::TimerHost : public QObject
{
public:
    explicit TimerHost(Component *owner) : m_owner(owner) {}

protected:
    void timerEvent(QTimerEvent *e) override
    {
        const int id = e->timerId();
        for (const auto &iv : m_owner->m_intervals) {
            if (iv.timerId == id) {
                if (iv.fn) iv.fn();
                return;
            }
        }
    }

private:
    Component *m_owner;
};

Component::Component() = default;

Component::~Component()
{
    if (m_loaded) {
        unload();
    } else {
        // Even if never loaded we still own our children.
        qDeleteAll(m_children);
        m_children.clear();
    }
    delete m_timerHost;
    m_timerHost = nullptr;
}

void Component::load()
{
    if (m_loaded) return;
    m_loaded = true;
    onload();
    // Auto-load any pre-existing children.
    for (Component *c : m_children) {
        c->load();
    }
}

void Component::unload()
{
    if (!m_loaded) return;

    // Children unload LIFO.
    for (int i = m_children.size() - 1; i >= 0; --i) {
        m_children[i]->unload();
    }

    // Stop all registered intervals.
    if (m_timerHost) {
        for (const auto &iv : m_intervals) {
            if (iv.timerId) m_timerHost->killTimer(iv.timerId);
        }
    }
    m_intervals.clear();

    // Disconnect all registered QObject connections.
    for (const auto &conn : m_connections) {
        QObject::disconnect(conn);
    }
    m_connections.clear();

    m_loaded = false;
    onunload();
}

void Component::addChild(Component *child)
{
    if (!child) return;
    m_children.append(child);
    if (m_loaded) {
        child->load();
    }
}

bool Component::removeChild(Component *child)
{
    if (!child) return false;
    const int idx = m_children.indexOf(child);
    if (idx < 0) return false;
    m_children.removeAt(idx);
    child->unload();
    delete child;
    return true;
}

int Component::registerInterval(int ms, std::function<void()> fn)
{
    if (!m_timerHost) m_timerHost = new TimerHost(this);
    const int id = m_timerHost->startTimer(ms);
    m_intervals.append({id, std::move(fn)});
    return id;
}

void Component::registerQObjectConnection(const QMetaObject::Connection &conn)
{
    m_connections.append(conn);
}

} // namespace Corbomite
