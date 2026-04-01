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
    m_boldPattern = QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*|__(.+?)__)"));
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

void MarkdownHighlighter::setCursorBlock(int blockNumber)
{
    if (m_cursorBlock == blockNumber) return;
    int oldBlock = m_cursorBlock;
    m_cursorBlock = blockNumber;

    // Only rehighlight the blocks that changed (old and new cursor vicinity)
    if (m_mode == Mode::LivePreview) {
        QTextDocument *doc = document();
        // Rehighlight old cursor vicinity
        for (int i = oldBlock - 1; i <= oldBlock + 1; ++i) {
            if (i >= 0) {
                QTextBlock b = doc->findBlockByNumber(i);
                if (b.isValid()) rehighlightBlock(b);
            }
        }
        // Rehighlight new cursor vicinity
        for (int i = blockNumber - 1; i <= blockNumber + 1; ++i) {
            if (i >= 0) {
                QTextBlock b = doc->findBlockByNumber(i);
                if (b.isValid()) rehighlightBlock(b);
            }
        }
    }
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
    bool nearCursor = (m_mode == Mode::Source) ||
        (m_mode == Mode::LivePreview &&
         blockNum >= m_cursorBlock - 1 && blockNum <= m_cursorBlock + 1);

    bool hideDelimiters = (m_mode == Mode::LivePreview && !nearCursor);

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
            highlightInlinePatterns(text, hideDelimiters);
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
            highlightInlinePatterns(text, hideDelimiters);
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
            highlightInlinePatterns(text, hideDelimiters);
            return;
        }
    }

    // Ordered list marker
    {
        static QRegularExpression olPattern(QStringLiteral(R"(^\s*\d+[.)]\s)"));
        QRegularExpressionMatch m = olPattern.match(text);
        if (m.hasMatch()) {
            setFormat(m.capturedStart(), m.capturedLength(), m_listMarkerFormat);
            highlightInlinePatterns(text, hideDelimiters);
            return;
        }
    }

    // Plain line
    highlightInlinePatterns(text, hideDelimiters);
}

void MarkdownHighlighter::highlightInlinePatterns(const QString &text, bool hideDelimiters)
{
    // Inline code: `code`
    {
        QRegularExpressionMatchIterator it = m_inlineCodePattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (hideDelimiters) {
                // Hide the backticks, format the content
                hideRange(m.capturedStart(), 1);  // opening `
                setFormat(m.capturedStart() + 1, m.capturedLength() - 2, m_inlineCodeFormat);
                hideRange(m.capturedEnd() - 1, 1);  // closing `
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_inlineCodeFormat);
            }
        }
    }

    // Bold: **text** or __text__
    {
        QRegularExpressionMatchIterator it = m_boldPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (hideDelimiters) {
                // Hide ** delimiters, apply bold to content
                int delimLen = text.mid(m.capturedStart(), 2) == QStringLiteral("**") ? 2 : 2;
                hideRange(m.capturedStart(), delimLen);
                setFormat(m.capturedStart() + delimLen,
                          m.capturedLength() - delimLen * 2, m_boldFormat);
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
            if (hideDelimiters) {
                hideRange(m.capturedStart(), 1);
                setFormat(m.capturedStart() + 1, m.capturedLength() - 2, m_italicFormat);
                hideRange(m.capturedEnd() - 1, 1);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_italicFormat);
            }
        }
    }

    // Strikethrough: ~~text~~
    {
        QRegularExpressionMatchIterator it = m_strikethroughPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (hideDelimiters) {
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
            if (hideDelimiters) {
                // Hide [[ and ]], show content as link
                hideRange(m.capturedStart(), 2);
                QString inner = m.captured(1);
                int pipePos = inner.indexOf(QLatin1Char('|'));
                if (pipePos >= 0) {
                    // [[target|display]] — hide target and pipe, show display
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
            if (hideDelimiters) {
                // Hide [ ] ( url ) — show only the link text
                hideRange(m.capturedStart(), 1);  // [
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
            if (hideDelimiters) {
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
            if (hideDelimiters) {
                hideRange(m.capturedStart(), 2);
                setFormat(m.capturedStart() + 2, m.capturedLength() - 4, m_highlightFormat);
                hideRange(m.capturedEnd() - 2, 2);
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_highlightFormat);
            }
        }
    }

    // Comment: %%text%%
    {
        QRegularExpressionMatchIterator it = m_commentPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (hideDelimiters) {
                // In live preview, comments are fully hidden
                hideRange(m.capturedStart(), m.capturedLength());
            } else {
                setFormat(m.capturedStart(), m.capturedLength(), m_commentFormat);
            }
        }
    }

    // Tag: #word
    {
        QRegularExpressionMatchIterator it = m_tagPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_tagFormat);
        }
    }
}

} // namespace Markoff
