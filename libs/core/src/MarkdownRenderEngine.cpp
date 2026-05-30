// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkdownRenderEngine.h"

#include "corbomite/core/SubpathExtract.h"

namespace Corbomite {

QString MarkdownRenderEngine::extractSubpath(const QString &markdown, const QString &subpath)
{
    // Behavior-preserving delegation to the shared free function so that
    // StyledRenderEngine (and any non-MarkdownRenderEngine caller) can reuse
    // the exact same subpath semantics.
    return extractMarkdownSubpath(markdown, subpath);
}

} // namespace Corbomite
