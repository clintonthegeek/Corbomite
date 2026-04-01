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

    // 3. Apply profile-specific CSS overrides
    QString styleOverrides = buildStylesheet(options);
    if (!styleOverrides.isEmpty()) {
        // Insert style overrides before </head>
        html.replace(QStringLiteral("</head>"),
                     QStringLiteral("<style>") + styleOverrides + QStringLiteral("</style></head>"));
    }

    // 4. Create QTextDocument from HTML
    auto doc = std::make_unique<QTextDocument>();
    doc->setHtml(html);

    return RenderedDocument::fromQTextDocument(std::move(doc));
}

QString RegexRenderEngine::buildStylesheet(const RenderOptions &options) const
{
    int fontSize = options.baseFontSizePt.value_or(m_profile.baseFontSizePt);
    int maxWidth = options.maxWidthPx.value_or(m_profile.maxWidthPx);
    int margin = options.marginPx.value_or(m_profile.marginPx);

    // Only emit overrides if they differ from the legacy renderer defaults
    // (legacy defaults: font-size 16px, max-width 700px, padding 20px)
    QString css;

    bool needsBody = (fontSize != 16 || maxWidth != 700 || margin != 20);
    if (needsBody) {
        css += QStringLiteral("body { ");
        css += QStringLiteral("font-size: %1px; ").arg(fontSize);
        if (maxWidth > 0) {
            css += QStringLiteral("max-width: %1px; ").arg(maxWidth);
        } else {
            css += QStringLiteral("max-width: none; ");
        }
        css += QStringLiteral("padding: %1px; ").arg(margin);
        css += QStringLiteral("} ");
    }

    return css;
}

} // namespace Corbomite
