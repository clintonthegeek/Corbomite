// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_RENDERSETTINGS_H
#define MARKOFF_RENDERSETTINGS_H

#include <QString>

namespace Markoff {

struct RenderSettings {
    int baseFontSizePt = 14;
    int maxWidthPx = 0;        // 0 = fill container
    int marginPx = 16;
    bool showFrontmatter = false;
    bool renderImages = true;
    bool renderCodeHighlighting = true;

    // Base path for resolving relative image/file paths
    // Empty = images shown as alt text only
    QString basePath;
};

} // namespace Markoff

#endif // MARKOFF_RENDERSETTINGS_H
