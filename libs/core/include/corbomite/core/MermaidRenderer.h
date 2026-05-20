// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_MERMAIDRENDERER_H
#define CORBOMITE_CORE_MERMAIDRENDERER_H

// TODO(port-foundation-exploration): Markoff::MermaidRenderer abstract was
// retired with the old leaves. Stubbed to a no-base class so this header
// compiles; consumers that pass this to a Markoff seam (NoteEditorWidget
// setMermaidRenderer, MainWindow) will need their own stubs until E5 lands a
// new Markoff::MermaidRenderer abstract.
// #include <markoff/MermaidRenderer.h>

#include <QByteArray>
#include <QString>

namespace Corbomite::Core {

/// Host-side concrete mermaid renderer. Wraps the `mmdr` Rust FFI.
class MermaidRenderer final
{
public:
    QByteArray renderSvg(const QString &source) const;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_MERMAIDRENDERER_H
