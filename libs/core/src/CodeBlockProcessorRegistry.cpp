// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/core/CodeBlockProcessorRegistry.h"

namespace Corbomite::Core {

CodeBlockProcessorRegistry::Handle
CodeBlockProcessorRegistry::registerLanguage(const QString &language,
                                             CodeBlockProcessorFn fn)
{
    const QString key = language.toLower();
    Handle h{m_nextId++, key};
    m_byLang.insert(key, Entry{h.id, std::move(fn)});
    return h;
}

void CodeBlockProcessorRegistry::unregister(const Handle &h)
{
    auto it = m_byLang.find(h.language);
    if (it != m_byLang.end() && it.value().id == h.id) {
        m_byLang.erase(it);
    }
}

bool CodeBlockProcessorRegistry::dispatch(const QString &language,
                                          const QString &source,
                                          void *node,
                                          const CodeBlockContext &ctx) const
{
    const auto it = m_byLang.constFind(language.toLower());
    if (it == m_byLang.constEnd()) {
        return false;
    }
    return it.value().fn(source, node, ctx);
}

} // namespace Corbomite::Core
