// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

namespace Corbomite {

enum class DecorationKind {
    /// Highlight a span of text (`payload["color"]`, `payload["style"]`).
    Highlight,
    /// Mount a small inline widget at the span (`payload["widget-id"]`).
    InlineWidget,
    /// Render a hover-only badge near the span
    /// (`payload["text"]`, `payload["icon"]`).
    HoverBadge,
};

/// POD describing a single decoration produced by a DecorationProvider.
/// `start` and `end` are character offsets into the source markdown
/// (UTF-16 code units; half-open `[start, end)` range). `kind` selects
/// the rendering strategy; `payload` carries kind-specific parameters.
struct Decoration {
    int start = 0;
    int end = 0;
    DecorationKind kind = DecorationKind::Highlight;
    QVariantMap payload;
};

/// Plugin-implemented producer of decorations for a given source.
/// Plugins subclass and register via `Plugin::registerEditorExtension`.
class DecorationProvider
{
public:
    virtual ~DecorationProvider() = default;

    /// Compute the decorations that apply to `markdown` at `sourcePath`.
    /// Called on every editor render pass; implementations should be
    /// fast and idempotent.
    virtual QList<Decoration> produceDecorations(const QString &sourcePath,
                                                    const QString &markdown) = 0;
};

} // namespace Corbomite
