// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_MARKDOWNRENDERCHILD_H
#define CORBOMITE_CORE_MARKDOWNRENDERCHILD_H

#include "corbomite/core/Component.h"

#include <markoff/MarkdownRenderChild.h>

#include <QPointer>
#include <QString>
#include <QWidget>

namespace Corbomite::Core {

/// Lifecycle-tied widget/scene-node subtree produced by post-processors,
/// code-block processors, and embed renderers. Auto-unloads when its
/// containing section is recycled by ReadingView's SectionRecyclePool.
///
/// Audit reference: `docs/obsidian-audit/domains/editor-markdown.md §10`.
///
/// Phase C1: now inherits `Markoff::MarkdownRenderChild` (for the DI
/// seam on the ReadingView side) in addition to Corbomite's `Component`
/// lifecycle. `setRenderedText` / `renderedText` are inherited from the
/// Markoff base; `mountInto` is overridden to keep the QPointer<QWidget>
/// host reference that Corbomite callers read via `hostWidget()`.
class MarkdownRenderChild : public Markoff::MarkdownRenderChild,
                            public Corbomite::Component
{
public:
    MarkdownRenderChild();
    ~MarkdownRenderChild() override;

    /// Attach this child's widget subtree to `host`. Host is referenced
    /// via QPointer so renderer code can detect host destruction without
    /// chasing dangling pointers.
    void mountInto(QWidget *host) override;
    QWidget *hostWidget() const;

private:
    QPointer<QWidget> m_host;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_MARKDOWNRENDERCHILD_H
