// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/core/EmbedRegistry.h"

#include "corbomite/core/MarkdownRenderChild.h"

#include <QFileInfo>

namespace Corbomite::Core {

EmbedRegistry::Handle EmbedRegistry::registerExtension(const QString &extension,
                                                      EmbedFactory fn)
{
    const QString key = extension.toLower();
    Handle h{m_nextId++, key};
    m_byExt.insert(key, Entry{h.id, std::move(fn)});
    return h;
}

void EmbedRegistry::unregister(const Handle &h)
{
    auto it = m_byExt.find(h.extension);
    if (it != m_byExt.end() && it.value().id == h.id) {
        m_byExt.erase(it);
    }
}

std::unique_ptr<MarkdownRenderChild>
EmbedRegistry::dispatch(const EmbedRequest &req) const
{
    const QString ext = QFileInfo(req.targetPath).suffix().toLower();
    const auto it = m_byExt.constFind(ext);
    if (it == m_byExt.constEnd()) {
        return nullptr;
    }
    return it.value().fn(req);
}

} // namespace Corbomite::Core
