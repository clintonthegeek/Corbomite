// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/canvas/CanvasDocumentRegistry.h"

namespace Corbomite::Canvas {

CanvasDocumentRegistry &CanvasDocumentRegistry::instance()
{
    static CanvasDocumentRegistry registry;
    return registry;
}

void CanvasDocumentRegistry::attach(const QString &path, CanvasDocument *doc)
{
    if (path.isEmpty() || !doc) return;
    m_docs.insert(path, doc);
}

void CanvasDocumentRegistry::detach(const QString &path, CanvasDocument *doc)
{
    const auto it = m_docs.find(path);
    if (it != m_docs.end() && it.value() == doc)
        m_docs.erase(it);
}

CanvasDocument *CanvasDocumentRegistry::documentFor(const QString &path) const
{
    return m_docs.value(path, nullptr);
}

} // namespace Corbomite::Canvas
