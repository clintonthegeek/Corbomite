// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFuture>
#include <QString>

class QObject;
class QWidget;

namespace Corbomite {

class MarkdownRenderer {
public:
    QString renderToHtml(const QString &markdown) const;

    // TODO: Replace regex renderer with cmark-gfm or other proper markdown
    // parser for full CommonMark spec compliance. The regex approach handles
    // common cases but will fail on edge cases like nested emphasis, reference
    // links, and complex list nesting. cmark-gfm provides a proper AST-based
    // pipeline with GFM extensions (tables, strikethrough, autolinks, task lists).

    /// Cluster B Phase 2 — plugin-facing widget renderer. Mirrors
    /// Obsidian's `MarkdownRenderer.render(app, md, el, sourcePath,
    /// component): Promise<void>` shape.
    ///
    /// Constructs a `Markoff::Reading::ReadingView` parented to `parent`
    /// and renders `markdown` into it. The returned `QFuture<void>`
    /// resolves once the synchronous content is laid out; math /
    /// mermaid / async embed children continue to render in the
    /// background and signal completion via the spawned widget tree.
    ///
    /// `lifetime` controls the rendered widget's lifespan: when
    /// `lifetime` is destroyed, the spawned widget is `deleteLater()`d.
    /// `sourcePath` is reserved for future relative-link resolution.
    ///
    /// Permissionless — plugin authors call this directly without
    /// permission gating; rendering is a side-effect-free read of
    /// `markdown`.
    static QFuture<void> render(const QString &markdown,
                                  QWidget *parent,
                                  const QString &sourcePath,
                                  QObject *lifetime);

private:
    QString processBlocks(const QString &markdown) const;
    QString processInline(const QString &text) const;
    QString wrapDocument(const QString &body) const;
    static QString escapeHtml(const QString &text);
    static QString defaultStylesheet();
};

} // namespace Corbomite
