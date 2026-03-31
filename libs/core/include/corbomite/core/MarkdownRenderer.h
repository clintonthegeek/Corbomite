// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class MarkdownRenderer {
public:
    QString renderToHtml(const QString &markdown) const;

    // TODO: Replace regex renderer with cmark-gfm or other proper markdown
    // parser for full CommonMark spec compliance. The regex approach handles
    // common cases but will fail on edge cases like nested emphasis, reference
    // links, and complex list nesting. cmark-gfm provides a proper AST-based
    // pipeline with GFM extensions (tables, strikethrough, autolinks, task lists).

private:
    QString processBlocks(const QString &markdown) const;
    QString processInline(const QString &text) const;
    QString wrapDocument(const QString &body) const;
    static QString escapeHtml(const QString &text);
    static QString defaultStylesheet();
};

} // namespace Corbomite
