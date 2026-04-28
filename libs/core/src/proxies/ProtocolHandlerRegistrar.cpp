// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/ProtocolHandlerRegistrar.h"

#include "corbomite/core/ProtocolHandlerRegistry.h"

namespace Corbomite {

ProtocolHandlerRegistrar::ProtocolHandlerRegistrar(
    ProtocolHandlerRegistry *registry, QString pluginId)
    : m_registry(registry), m_pluginId(std::move(pluginId))
{}

ProtocolHandlerRegistrar::~ProtocolHandlerRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredActions.size() - 1; i >= 0; --i) {
        m_registry->unregisterHandler(m_registeredActions.at(i));
    }
}

QString ProtocolHandlerRegistrar::registerHandler(const QString &localAction,
                                                    Handler handler)
{
    if (!m_registry || localAction.isEmpty() || !handler) return {};
    const QString fullAction = m_pluginId + QLatin1Char('.') + localAction;
    if (!m_registry->registerHandler(fullAction, std::move(handler))) return {};
    m_registeredActions.append(fullAction);
    return fullAction;
}

void ProtocolHandlerRegistrar::unregisterHandler(const QString &localAction)
{
    if (!m_registry) return;
    const QString fullAction = m_pluginId + QLatin1Char('.') + localAction;
    m_registry->unregisterHandler(fullAction);
    m_registeredActions.removeAll(fullAction);
}

} // namespace Corbomite
