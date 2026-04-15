// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_READINGVIEW_READINGSECTION_H
#define CORBOMITE_READINGVIEW_READINGSECTION_H

#include <QByteArray>

class QGraphicsItem;

namespace Corbomite::ReadingView {

/// A rendered unit of the reading pipeline — a contiguous span of source
/// markdown bounded by heading boundaries (or by document / frontmatter
/// bounds). One `QGraphicsItem` subtree per mounted section. Phase 4 uses
/// `renderedShape()` as the recycling key.
class ReadingSection
{
public:
    struct SourceRange { int from = 0; int to = 0; };

    ReadingSection();
    ~ReadingSection();

    SourceRange sourceRange() const { return m_sourceRange; }
    void setSourceRange(SourceRange range) { m_sourceRange = range; }

    // Heading metadata (if this section starts with a heading).
    int headingLevel() const { return m_headingLevel; }  // 0 = no heading
    void setHeadingLevel(int level) { m_headingLevel = level; }

    bool headingCollapsed() const { return m_headingCollapsed; }
    void setHeadingCollapsed(bool collapsed) { m_headingCollapsed = collapsed; }

    bool usesFrontMatter() const { return m_usesFrontMatter; }
    void setUsesFrontMatter(bool uses) { m_usesFrontMatter = uses; }

    bool isFrontMatterSection() const { return m_isFrontMatter; }
    void setIsFrontMatterSection(bool on) { m_isFrontMatter = on; }

    // Recycle key — populated by SectionLayout; Phase 4 will use this.
    QByteArray renderedShape() const { return m_renderedShape; }
    void setRenderedShape(const QByteArray &shape) { m_renderedShape = shape; }

    // The QGraphicsItem subtree for this section when mounted.
    QGraphicsItem *graphicsItem() const { return m_graphicsItem; }
    void setGraphicsItem(QGraphicsItem *item) { m_graphicsItem = item; }

private:
    SourceRange m_sourceRange{};
    int m_headingLevel = 0;
    bool m_headingCollapsed = false;
    bool m_usesFrontMatter = false;
    bool m_isFrontMatter = false;
    QByteArray m_renderedShape;
    QGraphicsItem *m_graphicsItem = nullptr;
};

} // namespace Corbomite::ReadingView

#endif // CORBOMITE_READINGVIEW_READINGSECTION_H
