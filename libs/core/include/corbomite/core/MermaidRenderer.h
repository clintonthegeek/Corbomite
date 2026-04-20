// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_MERMAIDRENDERER_H
#define CORBOMITE_CORE_MERMAIDRENDERER_H

#include <markoff/MermaidRenderer.h>

#include <QByteArray>
#include <QString>

namespace Corbomite::Core {

/// Host-side concrete `Markoff::MermaidRenderer` implementation. Wraps
/// the `mmdr` Rust FFI; CorbomiteApp plugs one of these into every
/// `ReadingView` it opens so the Markoff-side DI seam can render
/// mermaid diagrams. markoff-reading's Phase-C1 standalone Default
/// returns empty bytes — this concrete is what makes mermaid actually
/// render.
class MermaidRenderer final : public Markoff::MermaidRenderer
{
public:
    QByteArray renderSvg(const QString &source) const override;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_MERMAIDRENDERER_H
