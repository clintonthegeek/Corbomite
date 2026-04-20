// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/core/MermaidRenderer.h"

#include "mmdr_ffi.h"

#include <cstring>

namespace Corbomite::Core {

QByteArray MermaidRenderer::renderSvg(const QString &source) const
{
    if (source.trimmed().isEmpty()) return {};

    char *output = nullptr;
    const QByteArray in = source.toUtf8();
    const int result = mmdr_render_svg(in.constData(), &output);

    if (result != 0 || !output) {
        if (output) mmdr_free(output);
        return {};
    }

    QByteArray svg(output, static_cast<int>(std::strlen(output)));
    mmdr_free(output);
    return svg;
}

} // namespace Corbomite::Core
