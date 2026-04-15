// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_READINGVIEW_SECTIONLAYOUT_H
#define CORBOMITE_READINGVIEW_SECTIONLAYOUT_H

#include "corbomite/readingview/CodeBlockHighlighter.h"

#include <QList>
#include <QString>

class QGraphicsItemGroup;

namespace Corbomite::ReadingView {

class ReadingSection;
class StyleManager;
class CodeBlockHighlighter;

/// Lay out a single `ReadingSection` into a mounted QGraphicsItem subtree.
/// Phase 3a: synchronous, simple-stacking. Six content types supported —
/// headings, paragraphs, code blocks, lists, horizontal rules, blockquotes.
/// Tables, inline images, wiki-link rendering, math, and mermaid are
/// deferred to Phase 3b.
class SectionLayout
{
public:
    struct Context {
        StyleManager *styles = nullptr;      // not owned
        Theme theme = Theme::Light;
        qreal contentWidth = 800.0;          // pixel width for word-wrap
    };

    SectionLayout();
    ~SectionLayout();

    /// Lay out `section` using its source markdown. Returns a
    /// `QGraphicsItemGroup` root that the caller mounts into a scene and
    /// owns. Returns `nullptr` on failure.
    /// Populates `section.renderedShape()` with a SHA-256 digest of a
    /// deterministic type|text|… serialization — Phase 4's recycling key.
    QGraphicsItemGroup *layoutSection(ReadingSection &section,
                                      const QString &sectionMarkdown,
                                      const Context &ctx);

    /// Highlighters kept alive for the lifetime of this layout engine.
    /// (Each code block needs its own `CodeBlockHighlighter`; the engine
    /// parents them so the item-group's documents stay highlighted.)
    const QList<CodeBlockHighlighter *> &ownedHighlighters() const
    {
        return m_highlighters;
    }

private:
    QList<CodeBlockHighlighter *> m_highlighters;
};

} // namespace Corbomite::ReadingView

#endif // CORBOMITE_READINGVIEW_SECTIONLAYOUT_H
