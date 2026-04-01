// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CORBOMITE_MARKOFFRENDERENGINE_H
#define CORBOMITE_MARKOFFRENDERENGINE_H

#include "MarkdownRenderEngine.h"

namespace Corbomite {

class MarkoffRenderEngine : public MarkdownRenderEngine {
public:
    std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const override;
};

} // namespace Corbomite

#endif // CORBOMITE_MARKOFFRENDERENGINE_H
