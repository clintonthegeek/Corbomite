// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>

#include <functional>

namespace Corbomite {

/// Universal lifecycle base, modeled on Obsidian's `Component` class
/// (see `docs/obsidian-audit/domains/ui-bundle.md §1 components/Component.js`).
///
/// Contract:
///   - `load()` transitions to loaded and fires `onload()`. Idempotent.
///     Children are auto-loaded.
///   - `unload()` transitions to unloaded, fires `onunload()` on children
///     LIFO first, then on self. Cleans registered intervals and QObject
///     connections. Idempotent.
///   - `addChild(child)` takes ownership; if parent is loaded, the child
///     is loaded immediately.
///   - `removeChild(child)` unloads and deletes the child.
///   - `registerInterval(ms, fn)` runs `fn` every `ms` until unload.
///   - `registerQObjectConnection(conn)` disconnects on unload.
///
/// Intentional divergences from Obsidian:
///   - `registerDomEvent` (Obsidian) becomes `registerQObjectConnection`
///     — Qt's native event mechanism.
///   - `registerEvent(EventRef)` is provided later via `Corbomite::Events`.
///
/// Not a QObject. Subclasses that need signals should inherit QObject
/// separately; Component is intentionally small and copy-disabled.
class Component
{
public:
    Component();
    virtual ~Component();

    Component(const Component &) = delete;
    Component &operator=(const Component &) = delete;

    /// Load this component. Auto-loads any existing children. Idempotent.
    void load();

    /// Unload this component. Children unload LIFO first, then self.
    /// Registered intervals + QObject connections are cleaned. Idempotent.
    void unload();

    bool isLoaded() const { return m_loaded; }

    /// Take ownership of `child`. If this Component is already loaded,
    /// the child is loaded immediately. The child is deleted when
    /// `removeChild(child)` is called or when this Component is destroyed.
    void addChild(Component *child);

    /// Unload + delete a child. Returns true if the child was owned here.
    bool removeChild(Component *child);

    int childCount() const { return m_children.size(); }

    /// Run `fn` every `ms` milliseconds until unload. Returns an opaque
    /// id (stable within this Component's lifetime) that callers rarely
    /// need — registration is auto-cleaned on unload.
    int registerInterval(int ms, std::function<void()> fn);

    /// Disconnect `conn` on unload. Safe to call with a null connection.
    void registerQObjectConnection(const QMetaObject::Connection &conn);

    /// Run `fn` once on unload. Cleanups fire LIFO (reverse registration
    /// order) — matches Obsidian's `Component.register(cb)` semantics
    /// (domains/ui-bundle.md §1).
    void registerCleanup(std::function<void()> fn);

protected:
    virtual void onload() {}
    virtual void onunload() {}

private:
    struct Interval
    {
        int timerId = 0;
        std::function<void()> fn;
    };

    // We need a QObject to host timer events. Kept private + composition
    // so Component itself is not a QObject (avoids MOC + multiple-inheritance
    // pitfalls for subclasses that want their own Q_OBJECT).
    class TimerHost;

    bool m_loaded = false;
    QVector<Component *> m_children;
    QVector<Interval> m_intervals;
    QVector<QMetaObject::Connection> m_connections;
    QVector<std::function<void()>> m_cleanups;
    TimerHost *m_timerHost = nullptr;
};

} // namespace Corbomite
