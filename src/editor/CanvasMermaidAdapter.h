// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <markoff/canvas/MediaSeams.h>

namespace Corbomite::Core {
class MermaidRenderer;
}

namespace Corbomite {

/// Adapts `Corbomite::Core::MermaidRenderer` (SVG-bytes-out, wraps the
/// vendored `mmdr` Rust FFI) to the `Markoff::Canvas::MermaidRenderer` seam
/// (pixmap-out) that `Markoff::Canvas::EditorWidget::setMermaidRenderer`
/// expects. Rasterizes via `QSvgRenderer`, mirroring the SVG-to-QImage
/// pattern already used in `MarkdownRenderer.cpp`'s Reading-mode mermaid
/// path. A render failure (empty SVG bytes, invalid SVG) yields a null
/// QPixmap, matching the seam's documented miss behavior (falls back to
/// plain source text — no placeholder, no crash).
class CanvasMermaidAdapter : public Markoff::Canvas::MermaidRenderer
{
public:
    explicit CanvasMermaidAdapter(const Core::MermaidRenderer *renderer);

    QPixmap render(const QString &source) const override;

private:
    const Core::MermaidRenderer *m_renderer;
};

} // namespace Corbomite
