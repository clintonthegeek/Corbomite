// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class CommandRegistry;
class Command;

/// Command-registration facade for plugins with the "ui.commands" permission.
/// Mirrors Obsidian's `addCommand` auto-namespacing (`<pluginId>:<cmdId>`).
/// Stub — wire-up lands in Cluster Q Task 9.
class CommandRegistrar
{
public:
    CommandRegistrar(CommandRegistry *registry, QString pluginId)
        : m_registry(registry), m_pluginId(std::move(pluginId)) {}

    void addCommand(Command &cmd);
    bool removeCommand(const QString &localId);

    const QString &pluginId() const { return m_pluginId; }

private:
    CommandRegistry *m_registry;
    QString m_pluginId;
};

} // namespace Corbomite
