// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_MARKDOWNHIGHLIGHTER_H
#define MARKOFF_MARKDOWNHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include "SourceSpan.h"

namespace Markoff {

/// Markdown syntax highlighter driven by the parsed AST.
///
/// Instead of regex, this highlighter uses a pre-built span map
/// (from buildSpanMap()) that tells it exactly which byte ranges have
/// which formatting. The AST handles all nesting and precedence correctly.
class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MarkdownHighlighter(QTextDocument *parent);

    enum class Mode { Source, LivePreview };
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    /// Update the span map after a reparse.
    void setSpanMap(QList<SourceSpan> spans);

    /// Cursor position for per-element delimiter hiding.
    void setCursorPosition(int blockNumber, int columnInBlock);

protected:
    void highlightBlock(const QString &text) override;

private:
    void hideRange(int start, int length);
    bool cursorInRange(int cursorCol, int matchStart, int matchEnd) const;
    void applySpanFormat(const SourceSpan &span, int blockCharStart, int blockCharEnd,
                          bool shouldHideDelim, int cursorCol);

    Mode m_mode = Mode::Source;
    int m_cursorBlock = -1;
    int m_cursorColumn = -1;

    QList<SourceSpan> m_spans;

    // Format definitions
    QTextCharFormat m_headingFormat[6];
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_strikethroughFormat;
    QTextCharFormat m_inlineCodeFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_wikilinkFormat;
    QTextCharFormat m_blockquoteFormat;
    QTextCharFormat m_listMarkerFormat;
    QTextCharFormat m_codeBlockFormat;
    QTextCharFormat m_horizontalRuleFormat;
    QTextCharFormat m_mathFormat;
    QTextCharFormat m_highlightFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_tagFormat;
    QTextCharFormat m_frontmatterFormat;
    QTextCharFormat m_calloutFormat;
};

} // namespace Markoff
#endif
