// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/RegexRenderEngine.h"

#include <QTextDocument>

namespace Corbomite {

std::unique_ptr<RenderedDocument> RegexRenderEngine::render(
    const QString &markdown,
    const RenderOptions &options) const
{
    // 1. Extract subpath if requested
    QString md = markdown;
    if (!options.subpath.isEmpty()) {
        md = extractSubpath(markdown, options.subpath);
    }

    // 2. Render markdown to HTML via legacy renderer
    QString html = m_legacyRenderer.renderToHtml(md);

    // 3. Create QTextDocument from HTML with profile-specific CSS
    auto doc = std::make_unique<QTextDocument>();
    QString styleOverrides = buildStylesheet(options);
    if (!styleOverrides.isEmpty())
        doc->setDefaultStyleSheet(styleOverrides);
    doc->setHtml(html);

    return RenderedDocument::fromQTextDocument(std::move(doc));
}

QString RegexRenderEngine::buildStylesheet(const RenderOptions &options) const
{
    int fontSize = options.baseFontSizePt.value_or(m_profile.baseFontSizePt);
    int maxWidth = options.maxWidthPx.value_or(m_profile.maxWidthPx);
    int margin = options.marginPx.value_or(m_profile.marginPx);

    QString css;

    css += QStringLiteral("body { ");
    css += QStringLiteral("font-size: %1px; ").arg(fontSize);
    if (maxWidth > 0) {
        css += QStringLiteral("max-width: %1px; ").arg(maxWidth);
    } else {
        css += QStringLiteral("max-width: none; ");
    }
    css += QStringLiteral("padding: %1px; ").arg(margin);
    css += QStringLiteral("} ");

    return css;
}

} // namespace Corbomite
