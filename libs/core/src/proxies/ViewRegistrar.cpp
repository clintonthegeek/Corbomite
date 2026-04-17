// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/ViewRegistrar.h"

#include "corbomite/core/ViewRegistry.h"

namespace Corbomite {

ViewRegistrar::ViewRegistrar(ViewRegistry *registry) : m_registry(registry) {}

ViewRegistrar::~ViewRegistrar()
{
    if (!m_registry) return;
    if (!m_registeredExtensions.isEmpty())
        m_registry->unregisterExtensions(m_registeredExtensions);
    for (int i = m_registeredTypes.size() - 1; i >= 0; --i) {
        m_registry->unregisterView(m_registeredTypes.at(i));
    }
}

void ViewRegistrar::registerView(const QString &type, ViewFactory factory)
{
    if (!m_registry) return;
    m_registry->registerView(type, std::move(factory));
    if (!m_registeredTypes.contains(type)) m_registeredTypes.append(type);
}

void ViewRegistrar::registerExtensions(const QStringList &extensions,
                                       const QString &type)
{
    if (!m_registry) return;
    m_registry->registerExtensions(extensions, type);
    for (const auto &ext : extensions) {
        if (!m_registeredExtensions.contains(ext))
            m_registeredExtensions.append(ext);
    }
}

void ViewRegistrar::unregisterView(const QString &type)
{
    if (!m_registry) return;
    m_registry->unregisterView(type);
    m_registeredTypes.removeAll(type);
}

} // namespace Corbomite
