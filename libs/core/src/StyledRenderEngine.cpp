// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/LinkUtils.h"

#include "corbomite/core/EditingSession.h"
#include "corbomite/core/RenderedDocument.h"
#include "corbomite/core/RenderOptions.h"
#include "corbomite/core/SubpathExtract.h"

#include <markoff/core/MarkoffDocument.h>

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
    // 2026-10-01: use the Obsidian-faithful resolver, not the regex scrubber.
    // `extractMarkdownSubpath` (SubpathExtract.cpp) was the only thing available
    // because `resolveSubpath` demanded a Markoff::Document this path had no
    // reason to build; the source-only overload closes that. The scrubber
    // mishandles heading hierarchy, footnote definitions, and case-insensitive
    // block ids — all three are covered by the resolver's 20 tests.
    QString md = markdown;
    if (!options.subpath.isEmpty()) {
        const Corbomite::SubpathResolution r =
            Corbomite::resolveSubpath(markdown, options.subpath);
        if (r.startOffset >= 0) {
            md = (r.endOffset >= 0 && r.endOffset > r.startOffset)
                ? markdown.mid(r.startOffset, r.endOffset - r.startOffset)
                : markdown.mid(r.startOffset);
        }
        // Unresolved (r.startOffset < 0): fall back to the scrubber so an
        // exotic subpath still yields *something* rather than the whole note.
        else {
            md = extractMarkdownSubpath(markdown, options.subpath);
        }
    }

    auto doc = std::make_unique<QTextDocument>();
    // The byte overload of renderInto mints a private ephemeral key and
    // never asks EditingSessionSource. Paint the document this session
    // opened. openForRender imports only while the provider is waiting;
    // a ready live note is painted as-is, so this subpath slice is not
    // written into it. A refused open leaves the QTextDocument empty.
    const OpenDocument opened = openForRender(md.toUtf8(), options.documentKeyHint);
    if (opened.imported && opened.document)
        m_renderer.renderInto(doc.get(), opened.document.get());
    return RenderedDocument::fromQTextDocument(std::move(doc));
}

}  // namespace Corbomite
