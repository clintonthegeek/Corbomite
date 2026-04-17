// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

class CommandRegistry;
struct Command;

/// Command-registration facade for plugins with the "ui.commands" permission.
/// Auto-namespaces command ids as `<pluginId>:<localId>` (Obsidian quirk —
/// matches PluginInstance::addCommand). The proxy tracks every full id it
/// added and removes them all on destruction.
class CommandRegistrar
{
public:
    CommandRegistrar(CommandRegistry *registry, QString pluginId);
    ~CommandRegistrar();

    CommandRegistrar(const CommandRegistrar &) = delete;
    CommandRegistrar &operator=(const CommandRegistrar &) = delete;

    /// Mutates `cmd.id` in place to `<pluginId>:<id>` (matches Obsidian),
    /// then forwards to CommandRegistry::addCommand. Tracks the full id
    /// for cleanup on destruction.
    void addCommand(Command &cmd);

    /// Remove a command by *local* id (without the plugin prefix). Returns
    /// true if the command was registered by this proxy.
    bool removeCommand(const QString &localId);

    const QString &pluginId() const { return m_pluginId; }

private:
    CommandRegistry *m_registry;
    QString m_pluginId;
    QStringList m_registeredIds;
};

} // namespace Corbomite
