// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/DecorationProviderRegistrar.h"

#include "corbomite/core/DecorationProviderRegistry.h"

namespace Corbomite {

DecorationProviderRegistrar::DecorationProviderRegistrar(
    DecorationProviderRegistry *registry, QString pluginId)
    : m_registry(registry), m_pluginId(std::move(pluginId))
{}

DecorationProviderRegistrar::~DecorationProviderRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredIds.size() - 1; i >= 0; --i) {
        m_registry->unregisterProvider(m_registeredIds.at(i));
    }
}

QString DecorationProviderRegistrar::registerProvider(
    const QString &localId, DecorationProvider *provider)
{
    if (!m_registry || localId.isEmpty() || !provider) return {};
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    if (!m_registry->registerProvider(fullId, provider)) return {};
    m_registeredIds.append(fullId);
    return fullId;
}

void DecorationProviderRegistrar::unregisterProvider(const QString &localId)
{
    if (!m_registry) return;
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    m_registry->unregisterProvider(fullId);
    m_registeredIds.removeAll(fullId);
}

} // namespace Corbomite
