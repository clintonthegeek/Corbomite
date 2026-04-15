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

const Command *CommandRegistry::findCommand(const QString &id) const
{
    auto it = m_byId.constFind(id);
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
        if (isAvailableFor(it.value())) out.append(&it.value());
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
    if (cmd.editorCheckCallback) {
        return cmd.editorCheckCallback(true, m_activeEditor);
    }
    if (cmd.editorCallback) {
        return m_activeEditor != nullptr;
    }
    if (cmd.checkCallback) {
        return cmd.checkCallback(true);
    }
    return static_cast<bool>(cmd.callback);
}

bool CommandRegistry::executeById(const QString &id)
{
    auto *cmd = findCommand(id);
    if (!cmd) return false;
    if (!isAvailableFor(*cmd)) return false;

    if (cmd->editorCheckCallback) {
        return cmd->editorCheckCallback(false, m_activeEditor);
    }
    if (cmd->editorCallback) {
        cmd->editorCallback(m_activeEditor);
        return true;
    }
    if (cmd->checkCallback) {
        return cmd->checkCallback(false);
    }
    if (cmd->callback) {
        cmd->callback();
        return true;
    }
    return false;
}

} // namespace Corbomite
