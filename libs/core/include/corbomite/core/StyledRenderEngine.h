// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corbomite/core/MarkdownRenderEngine.h"
#include <markoff/styled/DocumentRenderer.h>
#include <memory>

namespace Markoff { class Theme; class ParserSession; }

namespace Corbomite {

/// MarkdownRenderEngine backed by Markoff::Styled::DocumentRenderer (headless,
/// read-only). Renders markdown bytes into a RenderedDocument's QTextDocument.
class StyledRenderEngine : public MarkdownRenderEngine {
public:
    StyledRenderEngine();
    ~StyledRenderEngine() override;
    /// Optional theme (non-owning, may be null → renderer's default palette).
    void setTheme(const Markoff::Theme *theme);

    std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const override;

private:
    Markoff::Styled::DocumentRenderer m_renderer;
    /// `[[note#heading]]` subpath resolution cold-parses the body through
    /// `Document::fromMarkdown` per render call; this session reuses
    /// unchanged subtrees and skips the parse + CST walk when the same body
    /// renders again (punch [storage][parsing][P2][follows 4e470f0c]).
    /// Mutable: render() is const (a contract method); the session is
    /// per-engine bookkeeping, not observable state. Owning unique_ptr with
    /// an out-of-line dtor keeps the class header light.
    mutable std::unique_ptr<Markoff::ParserSession> m_subpathSession;
};

}  // namespace Corbomite
