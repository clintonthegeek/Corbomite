// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MarkdownRenderEngine.h"
#include "MarkdownRenderer.h"

namespace Corbomite {

class RegexRenderEngine : public MarkdownRenderEngine {
public:
    std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const override;

private:
    MarkdownRenderer m_legacyRenderer;

    QString buildStylesheet(const RenderOptions &options) const;
};

} // namespace Corbomite
