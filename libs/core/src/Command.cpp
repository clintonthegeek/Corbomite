// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Command.h"

namespace Corbomite {

void CommandRegistry::addCommand(const Command &cmd)
{
    if (cmd.id.isEmpty()) return;
    if (!m_byId.contains(cmd.id)) {
        m_order.append(cmd.id);
    }
    m_byId.insert(cmd.id, cmd);
}

bool CommandRegistry::removeCommand(const QString &id)
{
    if (!m_byId.contains(id)) return false;
    m_byId.remove(id);
    m_order.removeAll(id);
    return true;
}

Command *CommandRegistry::findCommand(const QString &id)
{
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &it.value();
}

bool CommandRegistry::addAlias(const QString &aliasId, const QString &targetId)
{
    if (aliasId.isEmpty() || aliasId == targetId) return false;
    // Stricter than addCommand, which mirrors Obsidian's own missing duplicate
    // check (`app/own/1963-s5.js:22`). An alias's whole purpose is to be the
    // name a vault's config binds, so two plugins fighting over one is a bug to
    // surface, not a last-writer-wins to absorb. Plugin reload is unaffected:
    // CommandRegistrar removes its ids on destruction.
    if (m_byId.contains(aliasId)) return false;
    const auto target = m_byId.constFind(targetId);
    if (target == m_byId.constEnd() || !target->aliasOf.isEmpty()) return false;

    Command c;
    c.id = aliasId;
    c.name = target->name;
    c.icon = target->icon;
    c.aliasOf = targetId;
    addCommand(c);
    return true;
}

bool CommandRegistry::isAlias(const QString &id) const
{
    const auto it = m_byId.constFind(id);
    return it != m_byId.constEnd() && !it->aliasOf.isEmpty();
}

const Command *CommandRegistry::resolve(const Command &cmd, int hops) const
{
    const Command *c = &cmd;
    while (!c->aliasOf.isEmpty()) {
        if (hops-- <= 0) return nullptr;
        const auto it = m_byId.constFind(c->aliasOf);
        if (it == m_byId.constEnd()) return nullptr;
        c = &it.value();
    }
    return c;
}

const Command *CommandRegistry::findCommand(const QString &id) const
{
    const auto it = m_byId.constFind(id);
    return it == m_byId.constEnd() ? nullptr : &it.value();
}

QVector<Command *> CommandRegistry::listCommands()
{
    QVector<Command *> out;
    out.reserve(m_order.size());
    for (const auto &id : m_order) {
        auto it = m_byId.find(id);
        if (it != m_byId.end()) out.append(&it.value());
    }
    return out;
}

QVector<const Command *> CommandRegistry::listCommands() const
{
    QVector<const Command *> out;
    out.reserve(m_order.size());
    for (const auto &id : m_order) {
        auto it = m_byId.constFind(id);
        if (it != m_byId.constEnd()) out.append(&it.value());
    }
    return out;
}

QVector<Command *> CommandRegistry::listAvailable()
{
    QVector<Command *> out;
    out.reserve(m_order.size());
    for (const auto &id : m_order) {
        auto it = m_byId.find(id);
        if (it == m_byId.end()) continue;
        if (!it->aliasOf.isEmpty()) continue;
        if (isAvailableFor(it.value())) out.append(&it.value());
    }
    return out;
}

QVector<Command *> CommandRegistry::listPaletteCommands()
{
    QVector<Command *> out;
    for (Command *cmd : listAvailable()) {
        if (cmd && !cmd->paletteHidden)
            out.append(cmd);
    }
    return out;
}

bool CommandRegistry::isAvailable(const QString &id) const
{
    const auto *cmd = findCommand(id);
    if (!cmd) return false;
    return isAvailableFor(*cmd);
}

bool CommandRegistry::isAvailableFor(const Command &cmd) const
{
    const Command *c = resolve(cmd);
    if (!c) return false;
    if (c->editorCheckCallback) {
        return c->editorCheckCallback(true, m_activeEditor);
    }
    if (c->editorCallback) {
        return m_activeEditor != nullptr;
    }
    if (c->checkCallback) {
        return c->checkCallback(true);
    }
    return static_cast<bool>(c->callback);
}

bool CommandRegistry::executeById(const QString &id)
{
    auto *cmd = findCommand(id);
    if (!cmd) return false;
    const Command *c = resolve(*cmd);
    if (!c) return false;
    if (!isAvailableFor(*c)) return false;

    if (c->editorCheckCallback) {
        return c->editorCheckCallback(false, m_activeEditor);
    }
    if (c->editorCallback) {
        c->editorCallback(m_activeEditor);
        return true;
    }
    if (c->checkCallback) {
        return c->checkCallback(false);
    }
    if (c->callback) {
        c->callback();
        return true;
    }
    return false;
}

} // namespace Corbomite
