// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/StyledRenderEngine.h"

#include "corbomite/core/RenderedDocument.h"
#include "corbomite/core/RenderOptions.h"
#include "corbomite/core/SubpathExtract.h"

#include <QTextDocument>

namespace Corbomite {

StyledRenderEngine::StyledRenderEngine() = default;

void StyledRenderEngine::setTheme(const Markoff::Theme *theme)
{
    m_renderer.setTheme(theme);
}

std::unique_ptr<RenderedDocument> StyledRenderEngine::render(
    const QString &markdown, const RenderOptions &options) const
{
    const QString md = options.subpath.isEmpty()
        ? markdown
        : extractMarkdownSubpath(markdown, options.subpath);

    auto doc = std::make_unique<QTextDocument>();
    m_renderer.renderInto(doc.get(), md.toUtf8());
    return RenderedDocument::fromQTextDocument(std::move(doc));
}

}  // namespace Corbomite
