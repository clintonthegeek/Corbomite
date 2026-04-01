// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownHighlighter.h"

#include <QFont>
#include <QColor>
#include <QTextDocument>

namespace Markoff {

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // H1-H6: bold, scaled point sizes, dark blue
    const int headingSizes[6] = { 24, 20, 17, 15, 14, 13 };
    for (int i = 0; i < 6; ++i) {
        m_headingFormat[i].setFontWeight(QFont::Bold);
        m_headingFormat[i].setFontPointSize(headingSizes[i]);
        m_headingFormat[i].setForeground(QColor(QStringLiteral("#1a1a2e")));
    }

    // Bold
    m_boldFormat.setFontWeight(700);

    // Italic
    m_italicFormat.setFontItalic(true);

    // Strikethrough
    m_strikethroughFormat.setFontStrikeOut(true);

    // Inline code: monospace, light gray background
    QFont monoFont;
    const QStringList monoFamilies = {
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Fira Code"),
        QStringLiteral("monospace")
    };
    monoFont.setFamilies(monoFamilies);
    m_inlineCodeFormat.setFont(monoFont);
    m_inlineCodeFormat.setBackground(QColor(QStringLiteral("#f0f0f0")));

    // Link: blue, underline
    m_linkFormat.setForeground(QColor(QStringLiteral("#2196F3")));
    m_linkFormat.setFontUnderline(true);

    // Wikilink: purple, underline
    m_wikilinkFormat.setForeground(QColor(QStringLiteral("#7B1FA2")));
    m_wikilinkFormat.setFontUnderline(true);

    // Blockquote: gray
    m_blockquoteFormat.setForeground(QColor(QStringLiteral("#757575")));

    // List marker: teal
    m_listMarkerFormat.setForeground(QColor(QStringLiteral("#009688")));

    // Code block: light gray background
    m_codeBlockFormat.setBackground(QColor(QStringLiteral("#f5f5f5")));
    QFont codeBlockFont;
    codeBlockFont.setFamilies(monoFamilies);
    m_codeBlockFormat.setFont(codeBlockFont);

    // Horizontal rule: gray
    m_horizontalRuleFormat.setForeground(QColor(QStringLiteral("#9E9E9E")));

    // Math: green
    m_mathFormat.setForeground(QColor(QStringLiteral("#2E7D32")));

    // Highlight: yellow background
    m_highlightFormat.setBackground(QColor(QStringLiteral("#FFF9C4")));

    // Comment: light gray, italic
    m_commentFormat.setForeground(QColor(QStringLiteral("#BDBDBD")));
    m_commentFormat.setFontItalic(true);

    // Tag: orange
    m_tagFormat.setForeground(QColor(QStringLiteral("#E65100")));

    // Frontmatter: gray
    m_frontmatterFormat.setForeground(QColor(QStringLiteral("#78909C")));

    // Callout: teal, bold
    m_calloutFormat.setForeground(QColor(QStringLiteral("#00897B")));
    m_calloutFormat.setFontWeight(QFont::Bold);

    // Compile inline patterns
    m_boldPattern = QRegularExpression(QStringLiteral(R"((\*\*(.+?)\*\*|__(.+?)__))"));
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

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    const int prevState = previousBlockState();

    // --- Multi-line block state continuation ---

    if (prevState == FencedCode) {
        // Check if this line closes the fenced code block
        static QRegularExpression fenceClose(QStringLiteral(R"(^\s*```\s*$)"));
        if (fenceClose.match(text).hasMatch()) {
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
            setFormat(0, text.length(), m_frontmatterFormat);
            setCurrentBlockState(Normal);
        } else {
            setFormat(0, text.length(), m_frontmatterFormat);
            setCurrentBlockState(Frontmatter);
        }
        return;
    }

    if (prevState == BlockComment) {
        int closeIdx = text.indexOf(QStringLiteral("%%"));
        if (closeIdx >= 0) {
            setFormat(0, closeIdx + 2, m_commentFormat);
            setCurrentBlockState(Normal);
        } else {
            setFormat(0, text.length(), m_commentFormat);
            setCurrentBlockState(BlockComment);
        }
        return;
    }

    // --- Normal state: check block-level patterns ---

    setCurrentBlockState(Normal);

    // Frontmatter: only on the very first block (block number 0)
    if (currentBlock().blockNumber() == 0 && text == QStringLiteral("---")) {
        setFormat(0, text.length(), m_frontmatterFormat);
        setCurrentBlockState(Frontmatter);
        return;
    }

    // Fenced code block opening
    {
        static QRegularExpression fenceOpen(QStringLiteral(R"(^\s*```\s*\S*\s*$)"));
        if (fenceOpen.match(text).hasMatch()) {
            setFormat(0, text.length(), m_codeBlockFormat);
            setCurrentBlockState(FencedCode);
            return;
        }
    }

    // Horizontal rule: ***, ---, ___  (3+ chars, optional trailing spaces)
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
            int level = m.captured(1).length() - 1; // 0-based index into array
            setFormat(0, text.length(), m_headingFormat[level]);
            // Still apply inline patterns inside headings
            highlightInlinePatterns(text);
            return;
        }
    }

    // Callout (must check before plain blockquote)
    {
        static QRegularExpression calloutPattern(QStringLiteral(R"(^>\s*\[!)"));
        if (calloutPattern.match(text).hasMatch()) {
            setFormat(0, text.length(), m_calloutFormat);
            return;
        }
    }

    // Blockquote: one or more > markers
    {
        static QRegularExpression bqPattern(QStringLiteral(R"(^(\s*>)+)"));
        QRegularExpressionMatch m = bqPattern.match(text);
        if (m.hasMatch()) {
            setFormat(m.capturedStart(), m.capturedLength(), m_blockquoteFormat);
            highlightInlinePatterns(text);
            return;
        }
    }

    // Unordered list marker
    {
        static QRegularExpression ulPattern(QStringLiteral(R"(^\s*[-*+]\s)"));
        QRegularExpressionMatch m = ulPattern.match(text);
        if (m.hasMatch()) {
            setFormat(m.capturedStart(), m.capturedLength(), m_listMarkerFormat);
            highlightInlinePatterns(text);
            return;
        }
    }

    // Ordered list marker
    {
        static QRegularExpression olPattern(QStringLiteral(R"(^\s*\d+[.)]\s)"));
        QRegularExpressionMatch m = olPattern.match(text);
        if (m.hasMatch()) {
            setFormat(m.capturedStart(), m.capturedLength(), m_listMarkerFormat);
            highlightInlinePatterns(text);
            return;
        }
    }

    // Plain line — apply inline patterns
    highlightInlinePatterns(text);
}

void MarkdownHighlighter::highlightInlinePatterns(const QString &text)
{
    // Order matters: apply more specific/longer patterns first to avoid partial overlaps.

    // Inline code (applied first so inner content is not re-highlighted)
    {
        QRegularExpressionMatchIterator it = m_inlineCodePattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_inlineCodeFormat);
        }
    }

    // Bold (** or __)
    {
        QRegularExpressionMatchIterator it = m_boldPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_boldFormat);
        }
    }

    // Italic (* or _) — applied after bold so bold markers are already colored
    {
        QRegularExpressionMatchIterator it = m_italicPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_italicFormat);
        }
    }

    // Strikethrough
    {
        QRegularExpressionMatchIterator it = m_strikethroughPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_strikethroughFormat);
        }
    }

    // Wikilink [[...]]
    {
        QRegularExpressionMatchIterator it = m_wikilinkPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_wikilinkFormat);
        }
    }

    // Standard link [text](url)
    {
        QRegularExpressionMatchIterator it = m_linkPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_linkFormat);
        }
    }

    // Math inline $...$
    {
        QRegularExpressionMatchIterator it = m_mathInlinePattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_mathFormat);
        }
    }

    // Highlight ==...==
    {
        QRegularExpressionMatchIterator it = m_highlightPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_highlightFormat);
        }
    }

    // Inline comment %%...%%
    {
        QRegularExpressionMatchIterator it = m_commentPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_commentFormat);
        }
    }

    // Tag #word
    {
        QRegularExpressionMatchIterator it = m_tagPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_tagFormat);
        }
    }
}

} // namespace Markoff
