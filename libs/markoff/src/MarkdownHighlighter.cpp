// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownHighlighter.h"

#include <QFont>
#include <QColor>
#include <QTextDocument>

namespace Markoff {

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // H1-H6: bold, scaled point sizes, dark color
    const int headingSizes[6] = { 24, 20, 17, 15, 14, 13 };
    for (int i = 0; i < 6; ++i) {
        m_headingFormat[i].setFontWeight(QFont::Bold);
        m_headingFormat[i].setFontPointSize(headingSizes[i]);
        m_headingFormat[i].setForeground(QColor(QStringLiteral("#1a1a2e")));
    }

    m_boldFormat.setFontWeight(700);
    m_boldItalicFormat.setFontWeight(700);
    m_boldItalicFormat.setFontItalic(true);
    m_italicFormat.setFontItalic(true);
    m_strikethroughFormat.setFontStrikeOut(true);

    QFont monoFont;
    monoFont.setFamilies({QStringLiteral("JetBrains Mono"),
                          QStringLiteral("Fira Code"),
                          QStringLiteral("monospace")});
    m_inlineCodeFormat.setFont(monoFont);
    m_inlineCodeFormat.setBackground(QColor(QStringLiteral("#f0f0f0")));

    m_linkFormat.setForeground(QColor(QStringLiteral("#2196F3")));
    m_linkFormat.setFontUnderline(true);

    m_wikilinkFormat.setForeground(QColor(QStringLiteral("#7B1FA2")));
    m_wikilinkFormat.setFontUnderline(true);

    m_blockquoteFormat.setForeground(QColor(QStringLiteral("#757575")));
    m_listMarkerFormat.setForeground(QColor(QStringLiteral("#009688")));

    m_codeBlockFormat.setBackground(QColor(QStringLiteral("#f5f5f5")));
    QFont codeBlockFont;
    codeBlockFont.setFamilies({QStringLiteral("JetBrains Mono"),
                               QStringLiteral("Fira Code"),
                               QStringLiteral("monospace")});
    m_codeBlockFormat.setFont(codeBlockFont);

    m_horizontalRuleFormat.setForeground(QColor(QStringLiteral("#9E9E9E")));
    m_mathFormat.setForeground(QColor(QStringLiteral("#2E7D32")));
    m_highlightFormat.setBackground(QColor(QStringLiteral("#FFF9C4")));

    m_commentFormat.setForeground(QColor(QStringLiteral("#BDBDBD")));
    m_commentFormat.setFontItalic(true);

    m_tagFormat.setForeground(QColor(QStringLiteral("#E65100")));

    m_frontmatterFormat.setForeground(QColor(QStringLiteral("#78909C")));

    m_calloutFormat.setForeground(QColor(QStringLiteral("#00897B")));
    m_calloutFormat.setFontWeight(QFont::Bold);

    // Compile inline patterns
    // Bold-italic must be matched BEFORE bold and italic to avoid partial matches
    m_boldItalicPattern = QRegularExpression(QStringLiteral(R"(\*\*\*(.+?)\*\*\*|___(.+?)___)"));
    m_boldPattern = QRegularExpression(QStringLiteral(R"((?<!\*)\*\*(?!\*)(.+?)(?<!\*)\*\*(?!\*)|(?<!_)__(?!_)(.+?)(?<!_)__(?!_))"));
    m_italicPattern = QRegularExpression(QStringLiteral(R"((?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)|(?<!_)_(?!_)(.+?)(?<!_)_(?!_))"));
    m_strikethroughPattern = QRegularExpression(QStringLiteral(R"(~~(.+?)~~)"));
    m_inlineCodePattern = QRegularExpression(QStringLiteral(R"(`([^`]+)`)"));
    m_wikilinkPattern = QRegularExpression(QStringLiteral(R"(\[\[([^\]]+)\]\])"));
    m_linkPattern = QRegularExpression(QStringLiteral(R"(\[([^\]]+)\]\(([^)]+)\))"));
    m_mathInlinePattern = QRegularExpression(QStringLiteral(R"(\$([^$]+)\$)"));
    m_highlightPattern = QRegularExpression(QStringLiteral(R"(==(.+?)==)"));
    m_commentPattern = QRegularExpression(QStringLiteral(R"(%%(.+?)%%)"));
    m_tagPattern = QRegularExpression(QStringLiteral(R"((?<!\w)#[a-zA-Z][a-zA-Z0-9_/-]*)"));
}

void MarkdownHighlighter::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    rehighlight();
}

void MarkdownHighlighter::setCursorPosition(int blockNumber, int columnInBlock)
{
    bool blockChanged = (m_cursorBlock != blockNumber);
    bool columnChanged = (m_cursorColumn != columnInBlock);

    if (!blockChanged && !columnChanged) return;

    int oldBlock = m_cursorBlock;
    m_cursorBlock = blockNumber;
    m_cursorColumn = columnInBlock;

    if (m_mode == Mode::LivePreview) {
        QTextDocument *doc = document();

        if (blockChanged) {
            // Rehighlight old cursor block (now fully rendered)
            if (oldBlock >= 0) {
                QTextBlock b = doc->findBlockByNumber(oldBlock);
                if (b.isValid()) rehighlightBlock(b);
            }
        }

        // Always rehighlight current cursor block (column may have changed,
        // affecting which inline elements show their delimiters)
        {
            QTextBlock b = doc->findBlockByNumber(blockNumber);
            if (b.isValid()) rehighlightBlock(b);
        }
    }
}

bool MarkdownHighlighter::cursorInRange(int cursorCol, int matchStart, int matchEnd) const
{
    // Cursor is "in" the match if it's anywhere within the full match range
    // (including the delimiters themselves). This means clicking on the
    // rendered bold text OR its ** delimiters reveals the raw syntax.
    return cursorCol >= matchStart && cursorCol <= matchEnd;
}

/// Make a range of text invisible: transparent color, zero-width via letter spacing
void MarkdownHighlighter::hideRange(int start, int length)
{
    QTextCharFormat hidden;
    hidden.setForeground(Qt::transparent);
    hidden.setFontLetterSpacing(-100); // collapse to zero width
    hidden.setFontPointSize(1);        // tiny to minimize space
    setFormat(start, length, hidden);
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    const int prevState = previousBlockState();
    const int blockNum = currentBlock().blockNumber();

    // Determine if this block shows raw syntax or formatted content.
    bool isCursorLine = (blockNum == m_cursorBlock);
    bool nearCursor = (m_mode == Mode::Source) || (m_mode == Mode::LivePreview && isCursorLine);

    // For non-cursor lines: hide all delimiters.
    // For the cursor line: per-element hiding based on cursor column.
    bool hideDelimiters = (m_mode == Mode::LivePreview && !nearCursor);

    // Cursor column within this block (-1 if cursor is on a different line)
    int cursorCol = (m_mode == Mode::LivePreview && isCursorLine) ? m_cursorColumn : -1;

    // --- Multi-line block state continuation ---

    if (prevState == FencedCode) {
        static QRegularExpression fenceClose(QStringLiteral(R"(^\s*```\s*$)"));
        if (fenceClose.match(text).hasMatch()) {
            if (hideDelimiters)
                hideRange(0, text.length());
            else
                setFormat(0, text.length(), m_codeBlockFormat);
            setCurrentBlockState(Normal);
        } else {
            setFormat(0, text.length(), m_codeBlockFormat);
            setCurrentBlockState(FencedCode);
        }
        return;
    }

    if (prevState == Frontmatter) {
        if (text == QStringLiteral("---")) {
            if (hideDelimiters)
                hideRange(0, text.length());
            else
                setFormat(0, text.length(), m_frontmatterFormat);
            setCurrentBlockState(Normal);
        } else {
            if (hideDelimiters)
                hideRange(0, text.length());
            else
                setFormat(0, text.length(), m_frontmatterFormat);
            setCurrentBlockState(Frontmatter);
        }
        return;
    }

    if (prevState == BlockComment) {
        int closeIdx = text.indexOf(QStringLiteral("%%"));
        if (closeIdx >= 0) {
            if (hideDelimiters)
                hideRange(0, closeIdx + 2);
            else
                setFormat(0, closeIdx + 2, m_commentFormat);
            setCurrentBlockState(Normal);
        } else {
            if (hideDelimiters)
                hideRange(0, text.length());
            else
                setFormat(0, text.length(), m_commentFormat);
            setCurrentBlockState(BlockComment);
        }
        return;
    }

    // --- Normal state: check block-level patterns ---

    setCurrentBlockState(Normal);

    // Frontmatter: only on the very first block
    if (blockNum == 0 && text == QStringLiteral("---")) {
        if (hideDelimiters)
            hideRange(0, text.length());
        else
            setFormat(0, text.length(), m_frontmatterFormat);
        setCurrentBlockState(Frontmatter);
        return;
    }

    // Fenced code block opening
    {
        static QRegularExpression fenceOpen(QStringLiteral(R"(^\s*```\s*\S*\s*$)"));
        if (fenceOpen.match(text).hasMatch()) {
            if (hideDelimiters)
                hideRange(0, text.length());
            else
                setFormat(0, text.length(), m_codeBlockFormat);
            setCurrentBlockState(FencedCode);
            return;
        }
    }

    // Horizontal rule
    {
        static QRegularExpression hrPattern(QStringLiteral(R"(^(\*{3,}|-{3,}|_{3,})\s*$)"));
        if (hrPattern.match(text).hasMatch()) {
            setFormat(0, text.length(), m_horizontalRuleFormat);
            return;
        }
    }

    // Heading: ^#{1,6}\s
    {
        static QRegularExpression headingPattern(QStringLiteral(R"(^(#{1,6})\s)"));
        QRegularExpressionMatch m = headingPattern.match(text);
        if (m.hasMatch()) {
            int level = m.captured(1).length() - 1;
            QString contentAfterHashes = text.mid(m.capturedLength());

            if (hideDelimiters && !contentAfterHashes.trimmed().isEmpty()) {
                // Hide the ## prefix, apply heading format to the content
                hideRange(0, m.capturedLength());
                setFormat(m.capturedLength(), text.length() - m.capturedLength(),
                          m_headingFormat[level]);
            } else {
                // Show raw: either near cursor, or heading has no content
                // yet (just "## " with nothing after — user is still typing)
                setFormat(0, text.length(), m_headingFormat[level]);
            }
            highlightInlinePatterns(text, hideDelimiters, cursorCol);
            return;
        }
    }

    // Callout
    {
        static QRegularExpression calloutPattern(QStringLiteral(R"(^>\s*\[!)"));
        if (calloutPattern.match(text).hasMatch()) {
            setFormat(0, text.length(), m_calloutFormat);
            return;
        }
    }

    // Blockquote
    {
        static QRegularExpression bqPattern(QStringLiteral(R"(^(\s*>)+)"));
        QRegularExpressionMatch m = bqPattern.match(text);
        if (m.hasMatch()) {
            if (hideDelimiters) {
                hideRange(m.capturedStart(), m.capturedLength());
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_blockquoteFormat);
            }
            highlightInlinePatterns(text, hideDelimiters, cursorCol);
            return;
        }
    }

    // Unordered list marker
    {
        static QRegularExpression ulPattern(QStringLiteral(R"(^\s*([-*+])\s)"));
        QRegularExpressionMatch m = ulPattern.match(text);
        if (m.hasMatch()) {
            if (hideDelimiters) {
                // Replace the marker with a bullet character visually
                // (can't change text, but we can style the marker)
                setFormat(m.capturedStart(), m.capturedLength(), m_listMarkerFormat);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_listMarkerFormat);
            }
            highlightInlinePatterns(text, hideDelimiters, cursorCol);
            return;
        }
    }

    // Ordered list marker
    {
        static QRegularExpression olPattern(QStringLiteral(R"(^\s*\d+[.)]\s)"));
        QRegularExpressionMatch m = olPattern.match(text);
        if (m.hasMatch()) {
            setFormat(m.capturedStart(), m.capturedLength(), m_listMarkerFormat);
            highlightInlinePatterns(text, hideDelimiters, cursorCol);
            return;
        }
    }

    // Plain line
    highlightInlinePatterns(text, hideDelimiters, cursorCol);
}

void MarkdownHighlighter::highlightInlinePatterns(const QString &text,
                                                    bool hideDelimiters,
                                                    int cursorCol)
{
    // Helper: should we hide delimiters for this specific match?
    // - Non-cursor lines (hideDelimiters=true, cursorCol=-1): always hide
    // - Cursor line: hide unless cursor is inside this match's range
    auto shouldHide = [&](int matchStart, int matchEnd) -> bool {
        if (hideDelimiters) return true;
        if (cursorCol < 0) return false;  // source mode, never hide
        return !cursorInRange(cursorCol, matchStart, matchEnd);
    };

    // Bold-italic: ***text*** or ___text___ (must come before bold and italic)
    {
        QRegularExpressionMatchIterator it = m_boldItalicPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 3);
                setFormat(m.capturedStart() + 3,
                          m.capturedLength() - 6, m_boldItalicFormat);
                hideRange(m.capturedEnd() - 3, 3);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_boldItalicFormat);
            }
        }
    }

    // Bold: **text** or __text__
    // Use negative lookahead/behind to avoid matching inside ***bold-italic***
    {
        QRegularExpressionMatchIterator it = m_boldPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                int delimLen = 2;
                hideRange(m.capturedStart(), delimLen);
                // Apply bold to inner content — use format that preserves
                // existing properties (like code background) by only setting weight
                int innerStart = m.capturedStart() + delimLen;
                int innerLen = m.capturedLength() - delimLen * 2;
                for (int i = innerStart; i < innerStart + innerLen; ++i) {
                    QTextCharFormat fmt = format(i);
                    fmt.setFontWeight(700);
                    setFormat(i, 1, fmt);
                }
                hideRange(m.capturedEnd() - delimLen, delimLen);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_boldFormat);
            }
        }
    }

    // Italic: *text* or _text_
    {
        QRegularExpressionMatchIterator it = m_italicPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 1);
                int innerStart = m.capturedStart() + 1;
                int innerLen = m.capturedLength() - 2;
                for (int i = innerStart; i < innerStart + innerLen; ++i) {
                    QTextCharFormat fmt = format(i);
                    fmt.setFontItalic(true);
                    setFormat(i, 1, fmt);
                }
                hideRange(m.capturedEnd() - 1, 1);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_italicFormat);
            }
        }
    }

    // Inline code: `code` (after bold/italic so it can merge)
    {
        QRegularExpressionMatchIterator it = m_inlineCodePattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 1);
                // Merge code format with any existing bold/italic
                int innerStart = m.capturedStart() + 1;
                int innerLen = m.capturedLength() - 2;
                for (int i = innerStart; i < innerStart + innerLen; ++i) {
                    QTextCharFormat fmt = format(i);
                    fmt.merge(m_inlineCodeFormat);
                    setFormat(i, 1, fmt);
                }
                hideRange(m.capturedEnd() - 1, 1);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_inlineCodeFormat);
            }
        }
    }

    // Strikethrough: ~~text~~
    {
        QRegularExpressionMatchIterator it = m_strikethroughPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 2);
                setFormat(m.capturedStart() + 2, m.capturedLength() - 4, m_strikethroughFormat);
                hideRange(m.capturedEnd() - 2, 2);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_strikethroughFormat);
            }
        }
    }

    // Wikilink: [[target]] or [[target|display]]
    {
        QRegularExpressionMatchIterator it = m_wikilinkPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 2);
                QString inner = m.captured(1);
                int pipePos = inner.indexOf(QLatin1Char('|'));
                if (pipePos >= 0) {
                    hideRange(m.capturedStart() + 2, pipePos + 1);
                    setFormat(m.capturedStart() + 2 + pipePos + 1,
                              inner.length() - pipePos - 1, m_wikilinkFormat);
                } else {
                    setFormat(m.capturedStart() + 2, inner.length(), m_wikilinkFormat);
                }
                hideRange(m.capturedEnd() - 2, 2);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_wikilinkFormat);
            }
        }
    }

    // Standard link: [text](url)
    {
        QRegularExpressionMatchIterator it = m_linkPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 1);
                setFormat(m.capturedStart() + 1, m.captured(1).length(), m_linkFormat);
                hideRange(m.capturedStart() + 1 + m.captured(1).length(),
                          m.capturedLength() - m.captured(1).length() - 1);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_linkFormat);
            }
        }
    }

    // Math: $...$
    {
        QRegularExpressionMatchIterator it = m_mathInlinePattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 1);
                setFormat(m.capturedStart() + 1, m.capturedLength() - 2, m_mathFormat);
                hideRange(m.capturedEnd() - 1, 1);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_mathFormat);
            }
        }
    }

    // Highlight: ==text==
    {
        QRegularExpressionMatchIterator it = m_highlightPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (shouldHide(m.capturedStart(), m.capturedEnd())) {
                hideRange(m.capturedStart(), 2);
                setFormat(m.capturedStart() + 2, m.capturedLength() - 4, m_highlightFormat);
                hideRange(m.capturedEnd() - 2, 2);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_highlightFormat);
            }
        }
    }

    // Comment: %%text%%
    // Comments are always visible in the editor (both source and live preview).
    // They are only hidden in reading mode (handled by the Renderer).
    {
        QRegularExpressionMatchIterator it = m_commentPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_commentFormat);
        }
    }

    // Tag: #word (tags don't have delimiters to hide — always styled)
    {
        QRegularExpressionMatchIterator it = m_tagPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_tagFormat);
        }
    }
}

} // namespace Markoff
