// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkoffRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"
#include "corbomite/core/RenderProfile.h"
#include "corbomite/core/RenderOptions.h"

#include <QTextDocument>
#include <markoff/Document.h>
#include "Renderer.h"
#include <markoff/RenderSettings.h>

namespace Corbomite {

std::unique_ptr<RenderedDocument> MarkoffRenderEngine::render(
    const QString &markdown,
    const RenderOptions &options) const
{
    // Subpath extraction (use base class static utility)
    QString md = markdown;
    if (!options.subpath.isEmpty())
        md = extractSubpath(markdown, options.subpath);

    // Parse
    auto doc = Markoff::Document::fromMarkdown(md);

    // Translate Corbomite settings → Markoff settings
    Markoff::RenderSettings settings;
    settings.maxWidthPx = options.maxWidthPx.value_or(m_profile.maxWidthPx);
    settings.marginPx = options.marginPx.value_or(m_profile.marginPx);
    settings.showFrontmatter = m_profile.showFrontmatter;
    settings.renderImages = m_profile.renderImages;
    settings.renderCodeHighlighting = m_profile.renderCodeHighlighting;

    // Render
    Markoff::Renderer renderer;
    renderer.setSettings(settings);
    auto textDoc = renderer.renderToTextDocument(*doc);

    return RenderedDocument::fromQTextDocument(std::move(textDoc));
}

} // namespace Corbomite
