// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/HoverLinkSourceRegistrar.h"

#include "corbomite/core/HoverLinkSourceRegistry.h"

namespace Corbomite {

HoverLinkSourceRegistrar::HoverLinkSourceRegistrar(
    HoverLinkSourceRegistry *registry, QString pluginId)
    : m_registry(registry), m_pluginId(std::move(pluginId))
{}

HoverLinkSourceRegistrar::~HoverLinkSourceRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredIds.size() - 1; i >= 0; --i) {
        m_registry->unregisterSource(m_registeredIds.at(i));
    }
}

bool HoverLinkSourceRegistrar::registerSource(HoverLinkSource &source)
{
    if (!m_registry) return false;
    source.id = m_pluginId + QLatin1Char(':') + source.id;
    if (!m_registry->registerSource(source)) return false;
    m_registeredIds.append(source.id);
    return true;
}

void HoverLinkSourceRegistrar::unregisterSource(const QString &localId)
{
    if (!m_registry) return;
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    m_registry->unregisterSource(fullId);
    m_registeredIds.removeAll(fullId);
}

} // namespace Corbomite
