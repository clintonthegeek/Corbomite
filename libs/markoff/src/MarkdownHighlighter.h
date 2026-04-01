// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_MARKDOWNHIGHLIGHTER_H
#define MARKOFF_MARKDOWNHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

namespace Markoff {

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MarkdownHighlighter(QTextDocument *parent);

    /// In Source mode, all syntax is visible with coloring.
    /// In LivePreview mode, syntax delimiters are hidden (transparent)
    /// and formatting is applied to the content text directly.
    enum class Mode { Source, LivePreview };
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    /// The block number range where the cursor currently is.
    /// In LivePreview, blocks near the cursor show raw syntax.
    void setCursorBlock(int blockNumber);

protected:
    void highlightBlock(const QString &text) override;

private:
    void highlightInlinePatterns(const QString &text, bool hideDelimiters);
    void hideRange(int start, int length);

    Mode m_mode = Mode::Source;
    int m_cursorBlock = -1;

    // Block states
    enum BlockState {
        Normal = -1,
        FencedCode = 1,
        Frontmatter = 2,
        BlockComment = 3
    };

    // Formats
    QTextCharFormat m_headingFormat[6];  // H1-H6
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

    // Compiled patterns
    QRegularExpression m_boldPattern;
    QRegularExpression m_italicPattern;
    QRegularExpression m_strikethroughPattern;
    QRegularExpression m_inlineCodePattern;
    QRegularExpression m_linkPattern;
    QRegularExpression m_wikilinkPattern;
    QRegularExpression m_mathInlinePattern;
    QRegularExpression m_highlightPattern;
    QRegularExpression m_commentPattern;
    QRegularExpression m_tagPattern;
};

} // namespace Markoff
#endif
