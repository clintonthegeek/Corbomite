// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <QString>

#include "RenderProfile.h"
#include "RenderOptions.h"
#include "RenderedDocument.h"

namespace Corbomite {

class MarkdownRenderEngine {
public:
    virtual ~MarkdownRenderEngine() = default;

    virtual std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const = 0;

    void setProfile(const RenderProfile &profile) { m_profile = profile; }
    RenderProfile profile() const { return m_profile; }

    // Shared utility: extract content for a subpath from raw markdown
    static QString extractSubpath(const QString &markdown, const QString &subpath);

protected:
    RenderProfile m_profile;
};

} // namespace Corbomite
