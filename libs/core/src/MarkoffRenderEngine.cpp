// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkoffRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"

#include <QTextDocument>

namespace Corbomite {

// DEPRECATED: Stub implementation. Returns raw markdown as a plain
// QTextDocument. The Markoff rendering pipeline has been removed.
// Canvas card rendering needs a new approach (e.g., offscreen Editor
// widget or a dedicated card renderer).
std::unique_ptr<RenderedDocument> MarkoffRenderEngine::render(
    const QString &markdown,
    const RenderOptions &options) const
{
    QString md = markdown;
    if (!options.subpath.isEmpty())
        md = extractSubpath(markdown, options.subpath);

    auto doc = std::make_unique<QTextDocument>();
    doc->setPlainText(md);
    return RenderedDocument::fromQTextDocument(std::move(doc));
}

} // namespace Corbomite
