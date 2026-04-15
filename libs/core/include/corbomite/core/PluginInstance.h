// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/Command.h"
#include "corbomite/core/Component.h"

#include <QString>
#include <QStringList>

namespace Corbomite {

class CommandRegistry;

/// Scaffolded Plugin base. A full plugin-system implementation lives
/// in Cluster N (Plugin-ready surfaces); this stub exists now because
/// Command + Scope need a plugin-id context to match Obsidian's
/// `addCommand` namespacing behaviour (domains/plugin.md §3–§4).
///
/// Preserves Obsidian quirks:
///   - addCommand mutates `cmd.id` in place to `<pluginId>:<origId>`;
///     a second call double-namespaces (`pluginId:pluginId:origId`)
///     — intentionally preserved so plugin code expecting Obsidian's
///     behaviour works unchanged.
///   - unload() removes every command this plugin registered.
///
/// This is a Component, so the usual lifecycle (`load`/`unload`/
/// `addChild`/`registerInterval`/`registerQObjectConnection`) applies.
class PluginInstance : public Component
{
public:
    PluginInstance(QString pluginId, CommandRegistry *registry);
    ~PluginInstance() override;

    const QString &pluginId() const { return m_pluginId; }

    /// Register a command under this plugin's namespace. Mutates
    /// `cmd.id` in place — intentional Obsidian parity.
    void addCommand(Command &cmd);

    /// Remove by *local* id (without the plugin prefix). Returns true
    /// if the command was registered by this plugin.
    bool removeCommand(const QString &localId);

protected:
    void onunload() override;

private:
    QString m_pluginId;
    CommandRegistry *m_registry;
    /// Fully-qualified ids we added to the registry (cleaned on unload).
    QStringList m_registeredIds;
};

} // namespace Corbomite
