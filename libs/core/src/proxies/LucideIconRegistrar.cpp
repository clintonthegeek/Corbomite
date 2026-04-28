// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/LucideIconRegistrar.h"

#include "corbomite/core/LucideIconRegistry.h"

namespace Corbomite {

LucideIconRegistrar::LucideIconRegistrar(LucideIconRegistry *registry,
                                           QString pluginId)
    : m_registry(registry), m_pluginId(std::move(pluginId))
{}

LucideIconRegistrar::~LucideIconRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredNames.size() - 1; i >= 0; --i) {
        m_registry->removeIcon(m_registeredNames.at(i));
    }
}

QString LucideIconRegistrar::addIcon(const QString &localName,
                                       const QByteArray &svg)
{
    if (!m_registry || localName.isEmpty()) return {};
    const QString fullName = m_pluginId + QLatin1Char(':') + localName;
    m_registry->addIcon(fullName, svg);
    if (!m_registry->hasIcon(fullName)) return {};
    m_registeredNames.append(fullName);
    return fullName;
}

void LucideIconRegistrar::removeIcon(const QString &localName)
{
    if (!m_registry) return;
    const QString fullName = m_pluginId + QLatin1Char(':') + localName;
    m_registry->removeIcon(fullName);
    m_registeredNames.removeAll(fullName);
}

} // namespace Corbomite
