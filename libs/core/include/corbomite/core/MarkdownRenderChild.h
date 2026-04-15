// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_MARKDOWNRENDERCHILD_H
#define CORBOMITE_CORE_MARKDOWNRENDERCHILD_H

#include "corbomite/core/Component.h"

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
/// The accessors (`setRenderedText` / `renderedText` / `mountInto` /
/// `hostWidget`) are the minimum surface Phase 4 `EmbedRenderer` and the
/// Mermaid / math / code-block processors call against. Rendered text is
/// the source-string snapshot the renderer used — consumers read it to
/// decide whether a subsequent parse result can reuse the same child.
class MarkdownRenderChild : public Corbomite::Component
{
public:
    MarkdownRenderChild();
    ~MarkdownRenderChild() override;

    void setRenderedText(QString text);
    QString renderedText() const;

    /// Attach this child's widget subtree to `host`. Host is referenced
    /// via QPointer so renderer code can detect host destruction without
    /// chasing dangling pointers.
    void mountInto(QWidget *host);
    QWidget *hostWidget() const;

private:
    QString m_renderedText;
    QPointer<QWidget> m_host;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_MARKDOWNRENDERCHILD_H
