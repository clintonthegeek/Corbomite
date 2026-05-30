// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corbomite/core/MarkdownRenderEngine.h"
#include <markoff/styled/DocumentRenderer.h>

namespace Markoff { class Theme; }

namespace Corbomite {

/// MarkdownRenderEngine backed by Markoff::Styled::DocumentRenderer (headless,
/// read-only). Renders markdown bytes into a RenderedDocument's QTextDocument.
class StyledRenderEngine : public MarkdownRenderEngine {
public:
    StyledRenderEngine();
    /// Optional theme (non-owning, may be null → renderer's default palette).
    void setTheme(const Markoff::Theme *theme);

    std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const override;

private:
    Markoff::Styled::DocumentRenderer m_renderer;
};

}  // namespace Corbomite
