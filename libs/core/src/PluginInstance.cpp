// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PluginInstance.h"

namespace Corbomite {

PluginInstance::PluginInstance(QString pluginId, CommandRegistry *registry)
    : m_pluginId(std::move(pluginId)), m_registry(registry)
{}

PluginInstance::~PluginInstance() = default;

void PluginInstance::addCommand(Command &cmd)
{
    if (!m_registry) return;
    // Obsidian quirk: mutate in place. Second call double-namespaces.
    cmd.id = m_pluginId + QLatin1Char(':') + cmd.id;
    m_registry->addCommand(cmd);
    m_registeredIds.append(cmd.id);
}

bool PluginInstance::removeCommand(const QString &localId)
{
    if (!m_registry) return false;
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    if (!m_registry->removeCommand(fullId)) return false;
    m_registeredIds.removeAll(fullId);
    return true;
}

void PluginInstance::onunload()
{
    if (!m_registry) return;
    // Clean up all commands this plugin registered, in reverse order.
    for (int i = m_registeredIds.size() - 1; i >= 0; --i) {
        m_registry->removeCommand(m_registeredIds.at(i));
    }
    m_registeredIds.clear();
}

} // namespace Corbomite
