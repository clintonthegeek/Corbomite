// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <Qt>

#include <functional>
#include <memory>

class QKeyEvent;

namespace Corbomite {

/// Opaque handle for a `Scope::registerBinding` registration. Pass to
/// `Scope::unregister` to remove. Stale handles are a safe no-op.
class KeyBinding
{
public:
    KeyBinding() = default;

private:
    friend class Scope;
    struct Node;
    std::weak_ptr<Node> m_node;
    explicit KeyBinding(const std::shared_ptr<Node> &n) : m_node(n) {}
};

/// Hierarchical key-handler stack, modeled on Obsidian's `Scope` class
/// (see `docs/obsidian-audit/domains/core.md §1` and
/// `domains/platform.md` for the Keymap duplicate).
///
/// Each Scope owns a list of `(Modifiers, Key) → callback` bindings and
/// an optional parent. `handleKey` walks child-first → parent, first
/// callback that returns `true` consumes the event. A callback may
/// return `false` to fall through to the parent chain.
///
/// Preserved Obsidian quirk: a child binding that returns `true` masks
/// the parent's binding even when the child callback is effectively a
/// no-op. This is intentional — Obsidian relies on it for modal
/// containment (e.g. a modal's Esc suppresses the editor's Esc even
/// while the modal is still deciding whether to handle it).
///
/// Not a QObject — Scope instances are created and destroyed as Modal
/// / Menu objects open and close. Multiple bindings for the same key
/// are allowed and are tried in registration order until one returns
/// `true`.
class Scope
{
public:
    using Handler = std::function<bool(QKeyEvent *)>;

    Scope() = default;
    explicit Scope(Scope *parent) : m_parent(parent) {}
    ~Scope() = default;

    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

    /// Register a key binding. Returns a `KeyBinding` handle to pass to
    /// `unregister`. A callback returning `true` consumes the event;
    /// `false` falls through to siblings (same key in this scope) then
    /// the parent chain.
    KeyBinding registerBinding(Qt::KeyboardModifiers mods, int key,
                               Handler fn);

    /// Remove a previously registered binding. Idempotent (stale
    /// handles are a safe no-op).
    void unregister(const KeyBinding &handle);

    /// Walk child → parent. Returns true on first consumed event.
    bool handleKey(QKeyEvent *evt);

    Scope *parent() const { return m_parent; }

private:
    struct Key
    {
        Qt::KeyboardModifiers mods;
        int key;
        bool operator==(const Key &o) const
        {
            return mods == o.mods && key == o.key;
        }
    };
    friend size_t qHash(const Scope::Key &k, size_t seed) noexcept
    {
        return qHash(static_cast<uint>(k.mods), seed)
               ^ qHash(k.key, seed);
    }

    Scope *m_parent = nullptr;
    QHash<Key, QList<std::shared_ptr<KeyBinding::Node>>> m_bindings;
};

} // namespace Corbomite
