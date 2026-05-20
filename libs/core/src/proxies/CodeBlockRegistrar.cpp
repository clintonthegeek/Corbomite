// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/CodeBlockRegistrar.h"

namespace Corbomite {

CodeBlockRegistrar::CodeBlockRegistrar(
    Markoff::CodeBlockProcessorRegistry *registry)
    : m_registry(registry)
{}

CodeBlockRegistrar::~CodeBlockRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredLangs.size() - 1; i >= 0; --i) {
        m_registry->unregisterProcessor(m_registeredLangs.at(i));
    }
}

bool CodeBlockRegistrar::registerLanguage(
    const QString &lang, std::shared_ptr<Markoff::CodeBlockProcessor> proc)
{
    if (!m_registry || !proc) return false;
    if (m_registry->processorFor(lang)) return false;  // first-wins
    m_registry->registerProcessor(std::move(proc));
    m_registeredLangs.append(lang);
    return true;
}

void CodeBlockRegistrar::unregisterLanguage(const QString &lang)
{
    if (!m_registry) return;
    m_registry->unregisterProcessor(lang);
    m_registeredLangs.removeAll(lang);
}

} // namespace Corbomite
