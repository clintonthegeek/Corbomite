// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/EmbedRegistrar.h"

namespace Corbomite {

EmbedRegistrar::EmbedRegistrar(Markoff::EmbedRegistry *registry)
    : m_registry(registry)
{}

EmbedRegistrar::~EmbedRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredExts.size() - 1; i >= 0; --i) {
        m_registry->unregisterExtension(m_registeredExts.at(i));
    }
}

bool EmbedRegistrar::registerExtension(const QString &ext,
                                         Markoff::EmbedFactory factory)
{
    if (!m_registry) return false;
    const QString lower = ext.toLower();
    if (m_registry->hasExtension(lower)) return false;
    m_registry->registerExtension(lower, std::move(factory));
    m_registeredExts.append(lower);
    return true;
}

void EmbedRegistrar::unregisterExtension(const QString &ext)
{
    if (!m_registry) return;
    const QString lower = ext.toLower();
    m_registry->unregisterExtension(lower);
    m_registeredExts.removeAll(lower);
}

} // namespace Corbomite
