// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/core/PostProcessorRegistry.h"

#include <algorithm>

namespace Corbomite::Core {

PostProcessorRegistry::Handle
PostProcessorRegistry::registerProcessor(int priority, PostProcessorFn fn)
{
    Handle h{m_nextId++};
    m_entries.push_back({h.id, priority, m_nextSeq++, std::move(fn)});
    m_dirty = true;
    return h;
}

void PostProcessorRegistry::unregister(Handle h)
{
    auto it = std::remove_if(m_entries.begin(),
                             m_entries.end(),
                             [&](const Entry &e) { return e.id == h.id; });
    m_entries.erase(it, m_entries.end());
}

void PostProcessorRegistry::run(void *node,
                                const PostProcessorContext &ctx) const
{
    if (m_dirty) {
        std::stable_sort(m_entries.begin(),
                         m_entries.end(),
                         [](const Entry &a, const Entry &b) {
                             if (a.priority != b.priority) {
                                 return a.priority < b.priority;
                             }
                             return a.seq < b.seq;
                         });
        m_dirty = false;
    }
    for (const auto &e : m_entries) {
        e.fn(node, ctx);
    }
}

} // namespace Corbomite::Core
