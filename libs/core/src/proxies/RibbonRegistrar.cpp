// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/RibbonRegistrar.h"

#include "corbomite/core/RibbonHandle.h"

namespace Corbomite {

RibbonRegistrar::RibbonRegistrar(RibbonHandle *ribbon, QString pluginId)
    : m_ribbon(ribbon), m_pluginId(std::move(pluginId))
{}

RibbonRegistrar::~RibbonRegistrar()
{
    if (!m_ribbon) return;
    for (int i = m_registeredIds.size() - 1; i >= 0; --i) {
        m_ribbon->removeRibbonIcon(m_registeredIds.at(i));
    }
}

QString RibbonRegistrar::addRibbonIcon(const QString &localId,
                                         const QIcon &icon,
                                         const QString &title,
                                         std::function<void()> onActivated)
{
    if (!m_ribbon) return {};
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    const QString result = m_ribbon->addRibbonIcon(fullId, icon, title,
                                                     std::move(onActivated));
    if (result.isEmpty()) return {};
    m_registeredIds.append(fullId);
    return fullId;
}

bool RibbonRegistrar::removeRibbonIcon(const QString &localId)
{
    if (!m_ribbon) return false;
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    if (!m_ribbon->removeRibbonIcon(fullId)) return false;
    m_registeredIds.removeAll(fullId);
    return true;
}

} // namespace Corbomite
