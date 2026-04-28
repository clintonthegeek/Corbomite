// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/StatusBarRegistrar.h"

#include "corbomite/core/StatusBarRegistry.h"

namespace Corbomite {

StatusBarRegistrar::StatusBarRegistrar(StatusBarRegistry *registry,
                                         QString pluginId)
    : m_registry(registry), m_pluginId(std::move(pluginId))
{}

StatusBarRegistrar::~StatusBarRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredIds.size() - 1; i >= 0; --i) {
        m_registry->removeItem(m_registeredIds.at(i));
    }
}

QString StatusBarRegistrar::addItem(const QString &localId, QWidget *widget)
{
    if (!m_registry) return {};
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    if (!m_registry->addItem(fullId, widget)) return {};
    m_registeredIds.append(fullId);
    return fullId;
}

bool StatusBarRegistrar::removeItem(const QString &localId)
{
    if (!m_registry) return false;
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    if (!m_registry->removeItem(fullId)) return false;
    m_registeredIds.removeAll(fullId);
    return true;
}

} // namespace Corbomite
