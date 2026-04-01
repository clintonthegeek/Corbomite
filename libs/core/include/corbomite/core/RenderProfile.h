// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

struct RenderProfile {
    QString name;

    // Styling
    int baseFontSizePt = 14;
    int maxWidthPx = 0;           // 0 = no limit (fill container)
    int marginPx = 16;
    bool showFrontmatter = false;

    // Content
    bool renderImages = true;
    bool renderCodeHighlighting = true;

    static RenderProfile readingMode()
    {
        RenderProfile p;
        p.name = QStringLiteral("ReadingMode");
        p.baseFontSizePt = 16;
        p.maxWidthPx = 700;
        p.marginPx = 20;
        return p;
    }

    static RenderProfile canvasCard()
    {
        RenderProfile p;
        p.name = QStringLiteral("CanvasCard");
        p.baseFontSizePt = 11;
        p.maxWidthPx = 0;
        p.marginPx = 4;
        return p;
    }

    static RenderProfile hoverPreview()
    {
        RenderProfile p;
        p.name = QStringLiteral("HoverPreview");
        p.baseFontSizePt = 11;
        p.maxWidthPx = 0;
        p.marginPx = 8;
        p.renderImages = false;
        return p;
    }
};

} // namespace Corbomite
