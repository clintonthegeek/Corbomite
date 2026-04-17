// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/CommandRegistrar.h"

#include "corbomite/core/Command.h"

namespace Corbomite {

CommandRegistrar::CommandRegistrar(CommandRegistry *registry, QString pluginId)
    : m_registry(registry), m_pluginId(std::move(pluginId))
{}

CommandRegistrar::~CommandRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredIds.size() - 1; i >= 0; --i) {
        m_registry->removeCommand(m_registeredIds.at(i));
    }
}

void CommandRegistrar::addCommand(Command &cmd)
{
    if (!m_registry) return;
    cmd.id = m_pluginId + QLatin1Char(':') + cmd.id;
    m_registry->addCommand(cmd);
    m_registeredIds.append(cmd.id);
}

bool CommandRegistrar::removeCommand(const QString &localId)
{
    if (!m_registry) return false;
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    if (!m_registry->removeCommand(fullId)) return false;
    m_registeredIds.removeAll(fullId);
    return true;
}

} // namespace Corbomite
