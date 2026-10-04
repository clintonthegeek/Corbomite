// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Scope.h"

#include <QKeyEvent>

namespace Corbomite {

struct KeyBinding::Node
{
    Qt::KeyboardModifiers mods;
    int key = 0;
    Scope::Handler fn;
    bool active = true;
    bool catchAll = false;
};

KeyBinding Scope::registerBinding(Qt::KeyboardModifiers mods, int key,
                                  Handler fn)
{
    auto node = std::make_shared<KeyBinding::Node>();
    node->mods = mods;
    node->key = key;
    node->fn = std::move(fn);
    m_bindings[Key{mods, key}].append(node);
    return KeyBinding(node);
}

KeyBinding Scope::registerCatchAll(Handler fn)
{
    auto node = std::make_shared<KeyBinding::Node>();
    node->catchAll = true;
    node->fn = std::move(fn);
    m_catchAlls.append(node);
    return KeyBinding(node);
}

void Scope::unregister(const KeyBinding &handle)
{
    auto node = handle.m_node.lock();
    if (!node) return;
    node->active = false;
    if (node->catchAll) {
        m_catchAlls.removeOne(node);
        return;
    }
    auto it = m_bindings.find(Key{node->mods, node->key});
    if (it == m_bindings.end()) return;
    it->removeOne(node);
    if (it->isEmpty()) m_bindings.erase(it);
}

bool Scope::handleKey(QKeyEvent *evt)
{
    if (!evt) return false;

    const Qt::KeyboardModifiers mods = evt->modifiers();
    const int key = evt->key();

    auto it = m_bindings.find(Key{mods, key});
    if (it != m_bindings.end()) {
        // Snapshot to tolerate unregister during dispatch.
        const auto snapshot = *it;
        for (const auto &node : snapshot) {
            if (node && node->active && node->fn) {
                if (node->fn(evt)) return true;
            }
        }
    }

    const auto catchAlls = m_catchAlls;
    for (const auto &node : catchAlls) {
        if (node && node->active && node->fn) {
            if (node->fn(evt)) return true;
        }
    }

    // Fall through to parent.
    if (m_parent) return m_parent->handleKey(evt);
    return false;
}

} // namespace Corbomite
