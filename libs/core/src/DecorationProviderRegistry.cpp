// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/DecorationProviderRegistry.h"

namespace Corbomite {

DecorationProviderRegistry &DecorationProviderRegistry::instance()
{
    static DecorationProviderRegistry singleton;
    return singleton;
}

bool DecorationProviderRegistry::registerProvider(const QString &id,
                                                    DecorationProvider *provider)
{
    if (id.isEmpty() || !provider) return false;
    if (m_providers.contains(id)) return false;
    m_providers.insert(id, provider);
    m_order.append(id);
    return true;
}

void DecorationProviderRegistry::unregisterProvider(const QString &id)
{
    m_providers.remove(id);
    m_order.removeAll(id);
}

bool DecorationProviderRegistry::hasProvider(const QString &id) const
{
    return m_providers.contains(id);
}

QList<DecorationProvider *> DecorationProviderRegistry::providers() const
{
    QList<DecorationProvider *> out;
    out.reserve(m_order.size());
    for (const auto &id : m_order) out.append(m_providers.value(id));
    return out;
}

void DecorationProviderRegistry::clearForTesting()
{
    m_providers.clear();
    m_order.clear();
}

} // namespace Corbomite
