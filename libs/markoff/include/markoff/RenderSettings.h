// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_RENDERSETTINGS_H
#define MARKOFF_RENDERSETTINGS_H

namespace Markoff {

struct RenderSettings {
    int maxWidthPx = 0;        // 0 = fill container
    int marginPx = 16;
    bool showFrontmatter = false;
    bool renderImages = true;
    bool renderCodeHighlighting = true;
};

} // namespace Markoff

#endif // MARKOFF_RENDERSETTINGS_H
