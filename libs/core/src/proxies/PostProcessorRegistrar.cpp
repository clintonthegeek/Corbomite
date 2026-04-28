// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/PostProcessorRegistrar.h"

namespace Corbomite {

PostProcessorRegistrar::PostProcessorRegistrar(
    Corbomite::Core::PostProcessorRegistry *registry)
    : m_registry(registry)
{}

PostProcessorRegistrar::~PostProcessorRegistrar()
{
    if (!m_registry) return;
    for (int i = m_handles.size() - 1; i >= 0; --i) {
        m_registry->unregister(m_handles.at(i));
    }
}

Corbomite::Core::PostProcessorRegistry::Handle
PostProcessorRegistrar::registerProcessor(int priority,
                                            Corbomite::Core::PostProcessorFn fn)
{
    if (!m_registry) return {};
    auto handle = m_registry->registerProcessor(priority, std::move(fn));
    m_handles.append(handle);
    return handle;
}

void PostProcessorRegistrar::unregister(
    Corbomite::Core::PostProcessorRegistry::Handle handle)
{
    if (!m_registry) return;
    m_registry->unregister(handle);
    for (int i = m_handles.size() - 1; i >= 0; --i) {
        if (m_handles.at(i).id == handle.id) {
            m_handles.remove(i);
            break;
        }
    }
}

} // namespace Corbomite
