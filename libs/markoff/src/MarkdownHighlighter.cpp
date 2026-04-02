// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownHighlighter.h"

#include <QFont>
#include <QColor>
#include <QTextDocument>
#include <QTextBlock>
#include <QRegularExpression>

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
    m_inlineCodeFormat.setFontFamilies({QStringLiteral("JetBrains Mono"),
                                        QStringLiteral("Fira Code"),
                                        QStringLiteral("monospace")});
    m_inlineCodeFormat.setBackground(QColor(QStringLiteral("#f0f0f0")));

    m_linkFormat.setForeground(QColor(QStringLiteral("#2196F3")));
    m_linkFormat.setFontUnderline(true);

    m_wikilinkFormat.setForeground(QColor(QStringLiteral("#7B1FA2")));
    m_wikilinkFormat.setFontUnderline(true);

    m_blockquoteFormat.setForeground(QColor(QStringLiteral("#757575")));
    m_listMarkerFormat.setForeground(QColor(QStringLiteral("#009688")));

    QFont codeBlockFont;
    codeBlockFont.setFamilies({QStringLiteral("JetBrains Mono"),
                               QStringLiteral("Fira Code"),
                               QStringLiteral("monospace")});
    m_codeBlockFormat.setFont(codeBlockFont);
    m_codeBlockFormat.setBackground(QColor(QStringLiteral("#f5f5f5")));

    m_horizontalRuleFormat.setForeground(QColor(QStringLiteral("#9E9E9E")));
    m_mathFormat.setForeground(QColor(QStringLiteral("#2E7D32")));
    m_highlightFormat.setBackground(QColor(QStringLiteral("#FFF9C4")));

    m_commentFormat.setForeground(QColor(QStringLiteral("#BDBDBD")));
    m_commentFormat.setFontItalic(true);

    m_tagFormat.setForeground(QColor(QStringLiteral("#E65100")));
    m_frontmatterFormat.setForeground(QColor(QStringLiteral("#78909C")));

    m_calloutFormat.setForeground(QColor(QStringLiteral("#00897B")));
    m_calloutFormat.setFontWeight(QFont::Bold);
}

void MarkdownHighlighter::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    rehighlight();
}

void MarkdownHighlighter::setSpanMap(QList<SourceSpan> spans)
{
    m_spans = std::move(spans);
    // Don't call rehighlight() here — the document change that triggered
    // the reparse will also trigger Qt's automatic rehighlight. Calling
    // rehighlight() explicitly would re-modify the document, triggering
    // another contentsChanged → reparse → setSpanMap → infinite loop.
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
        if (blockChanged && oldBlock >= 0) {
            QTextBlock b = doc->findBlockByNumber(oldBlock);
            if (b.isValid()) rehighlightBlock(b);
        }
        {
            QTextBlock b = doc->findBlockByNumber(blockNumber);
            if (b.isValid()) rehighlightBlock(b);
        }
    }
}

void MarkdownHighlighter::hideRange(int start, int length)
{
    QTextCharFormat hidden;
    hidden.setForeground(Qt::transparent);
    hidden.setFontLetterSpacing(-100);
    hidden.setFontPointSize(1);
    setFormat(start, length, hidden);
}

bool MarkdownHighlighter::cursorInRange(int cursorCol, int matchStart, int matchEnd) const
{
    return cursorCol >= matchStart && cursorCol <= matchEnd;
}

void MarkdownHighlighter::applySpanFormat(const SourceSpan &span,
                                           int blockCharStart, int blockCharEnd,
                                           bool shouldHideDelim, int cursorCol)
{
    // Compute the portion of this span that overlaps the current block
    int spanCharStart = span.charOffset;
    int spanCharEnd = span.charOffset + span.charLength;

    // Clip to block range
    int localStart = qMax(spanCharStart, blockCharStart) - blockCharStart;
    int localEnd = qMin(spanCharEnd, blockCharEnd) - blockCharStart;
    if (localStart >= localEnd || localStart < 0)
        return;
    int localLen = localEnd - localStart;

    // Delimiter spans
    if (span.isDelimiter) {
        bool hide = shouldHideDelim;
        // Per-element: on cursor line, only hide if cursor is NOT within
        // the parent formatting element's range (not just adjacent to
        // this specific delimiter character)
        if (!hide && cursorCol >= 0) {
            if (span.parentCharStart >= 0 && span.parentCharEnd >= 0) {
                // Use parent element range: show delimiters when cursor
                // is anywhere between opening and closing delimiters
                int parentLocalStart = span.parentCharStart - blockCharStart;
                int parentLocalEnd = span.parentCharEnd - blockCharStart;
                hide = !cursorInRange(cursorCol, parentLocalStart, parentLocalEnd);
            } else {
                // No parent range (block-level delimiters like ##)
                hide = !cursorInRange(cursorCol, localStart, localEnd);
            }
        }

        if (hide) {
            hideRange(localStart, localLen);
        } else {
            // Source mode or cursor-adjacent: show delimiter with context coloring
            if (span.isHeading && span.headingLevel >= 1 && span.headingLevel <= 6) {
                setFormat(localStart, localLen, m_headingFormat[span.headingLevel - 1]);
            } else if (span.code) {
                setFormat(localStart, localLen, m_inlineCodeFormat);
            } else if (span.bold && span.italic) {
                QTextCharFormat fmt;
                fmt.setFontWeight(700);
                fmt.setFontItalic(true);
                setFormat(localStart, localLen, fmt);
            } else if (span.bold) {
                setFormat(localStart, localLen, m_boldFormat);
            } else if (span.italic) {
                setFormat(localStart, localLen, m_italicFormat);
            } else if (span.strikethrough) {
                setFormat(localStart, localLen, m_strikethroughFormat);
            } else if (span.math || span.mathDisplay) {
                setFormat(localStart, localLen, m_mathFormat);
            } else if (span.highlight) {
                setFormat(localStart, localLen, m_highlightFormat);
            } else if (span.comment) {
                setFormat(localStart, localLen, m_commentFormat);
            } else if (span.isBlockquoteMarker) {
                setFormat(localStart, localLen, m_blockquoteFormat);
            } else if (span.isWikilink) {
                setFormat(localStart, localLen, m_wikilinkFormat);
            } else if (span.isLink) {
                setFormat(localStart, localLen, m_linkFormat);
            }
        }
        return;
    }

    // Content spans: apply formatting by merging onto existing format
    // (so nested formatting accumulates: bold + code = bold monospace)
    // Skip spans with no formatting flags — they're plain text and would
    // overwrite formatting already applied by other spans.
    bool hasAnyFormat = span.bold || span.italic || span.strikethrough ||
        span.code || span.math || span.mathDisplay || span.highlight ||
        span.comment || span.isTag || span.isLink || span.isWikilink ||
        span.isImage || span.isHeading || span.isHorizontalRule ||
        span.isListMarker || span.isBlockquoteMarker || span.isFrontmatter ||
        span.isBlockquote || span.isFootnoteRef || span.isCodeBlockContent;
    if (!hasAnyFormat)
        return;

    for (int i = localStart; i < localEnd; ++i) {
        QTextCharFormat fmt = format(i);

        if (span.isHeading && span.headingLevel >= 1 && span.headingLevel <= 6) {
            const auto &hfmt = m_headingFormat[span.headingLevel - 1];
            fmt.setFontWeight(hfmt.fontWeight());
            fmt.setFontPointSize(hfmt.fontPointSize());
            fmt.setForeground(hfmt.foreground());
        }

        if (span.bold)
            fmt.setFontWeight(700);
        if (span.italic)
            fmt.setFontItalic(true);
        if (span.strikethrough)
            fmt.setFontStrikeOut(true);
        if (span.code)
            fmt.merge(m_inlineCodeFormat);
        if (span.math || span.mathDisplay)
            fmt.setForeground(m_mathFormat.foreground());
        if (span.highlight)
            fmt.setBackground(m_highlightFormat.background());
        if (span.comment) {
            fmt.setForeground(m_commentFormat.foreground());
            fmt.setFontItalic(true);
        }
        if (span.isTag)
            fmt.setForeground(m_tagFormat.foreground());
        if (span.isLink)
            fmt.merge(m_linkFormat);
        if (span.isWikilink)
            fmt.merge(m_wikilinkFormat);
        if (span.isHorizontalRule) {
            // Make text transparent — decoration painter draws the line
            fmt.setForeground(Qt::transparent);
        }
        if (span.isFootnoteRef) {
            // Hide the ^ character (first char) and superscript the number
            hideRange(localStart, 1); // hide ^
            for (int j = localStart + 1; j < localEnd; ++j) {
                QTextCharFormat sfmt = format(j);
                sfmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
                sfmt.setForeground(QColor(QStringLiteral("#1565C0")));
                setFormat(j, 1, sfmt);
            }
            return;
        }
        if (span.isListMarker)
            fmt.setForeground(m_listMarkerFormat.foreground());
        if (span.isCodeBlockContent)
            fmt.setFontFamilies({QStringLiteral("JetBrains Mono"),
                                 QStringLiteral("Fira Code"),
                                 QStringLiteral("monospace")});
        if (span.isBlockquote && !span.isHeading && !span.bold && !span.italic)
            fmt.setForeground(m_blockquoteFormat.foreground());

        setFormat(i, 1, fmt);
    }
}

// Helper class for KSyntaxHighlighting integration
class CodeBlockHighlighter : public KSyntaxHighlighting::AbstractHighlighter {
public:
    struct Run { int start, length; QColor color; bool bold, italic; };
    QList<Run> runs;
    QString m_line;

    KSyntaxHighlighting::State processLine(const QString &text,
                                            const KSyntaxHighlighting::State &state) {
        m_line = text;
        runs.clear();
        return highlightLine(text, state);
    }
protected:
    void applyFormat(int offset, int length,
                     const KSyntaxHighlighting::Format &fmt) override {
        Run r;
        r.start = offset;
        r.length = length;
        r.color = fmt.isDefaultTextStyle(theme())
                      ? QColor(0x33, 0x33, 0x33)
                      : fmt.textColor(theme());
        r.bold = fmt.isBold(theme());
        r.italic = fmt.isItalic(theme());
        runs.append(r);
    }
};

void MarkdownHighlighter::highlightCodeBlock(const QString &text,
                                              const DecoratedRange &dr, int blockNum)
{
    if (dr.language.isEmpty())
        return;

    auto def = m_syntaxRepo.definitionForName(dr.language);
    if (!def.isValid())
        def = m_syntaxRepo.definitionForFileName(QStringLiteral("file.") + dr.language);
    if (!def.isValid())
        return;

    // Use block state to carry KSyntaxHighlighting state across lines.
    // We store a state index in the QTextBlock's userState. For simplicity,
    // we re-highlight from the first code line to the current one.
    // This is O(n) per block but code blocks are typically short.
    CodeBlockHighlighter hl;
    hl.setDefinition(def);
    hl.setTheme(m_syntaxRepo.defaultTheme(KSyntaxHighlighting::Repository::LightTheme));

    KSyntaxHighlighting::State state;
    QTextBlock b = document()->findBlockByNumber(dr.firstBlock + 1); // skip opening fence
    while (b.isValid() && b.blockNumber() < blockNum) {
        state = hl.processLine(b.text(), state);
        b = b.next();
    }

    // Now highlight the current line
    state = hl.processLine(text, state);
    for (const auto &run : hl.runs) {
        QTextCharFormat fmt = format(run.start);
        fmt.setForeground(run.color);
        if (run.bold) fmt.setFontWeight(QFont::Bold);
        if (run.italic) fmt.setFontItalic(true);
        setFormat(run.start, run.length, fmt);
    }
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    if (text.isEmpty())
        return;

    // If the span map is stale (text changed but reparse hasn't run yet),
    // skip re-formatting. The block keeps its pre-edit formatting which is
    // close enough until the debounced reparse fixes everything.
    if (m_spanMapStale)
        return;

    const int blockNum = currentBlock().blockNumber();
    const int blockPos = currentBlock().position();
    const int blockLen = text.length();

    // Determine cursor-related visibility
    bool isCursorLine = (blockNum == m_cursorBlock);
    bool hideDelimiters = (m_mode == Mode::LivePreview && !isCursorLine);
    int cursorCol = (m_mode == Mode::LivePreview && isCursorLine) ? m_cursorColumn : -1;


    // Find all spans that overlap this block's character range
    int blockCharStart = blockPos;
    int blockCharEnd = blockPos + blockLen;

    // Track blockquote depth for this block (for indentation)
    int maxBlockquoteDepth = 0;

    for (const SourceSpan &span : m_spans) {
        int spanEnd = span.charOffset + span.charLength;
        if (spanEnd <= blockCharStart)
            continue;
        if (span.charOffset >= blockCharEnd)
            break;  // spans are sorted by offset

        if (span.blockquoteDepth > maxBlockquoteDepth)
            maxBlockquoteDepth = span.blockquoteDepth;

        applySpanFormat(span, blockCharStart, blockCharEnd, hideDelimiters, cursorCol);
    }

    // Make table blocks transparent (embedded QTableWidget renders on top)
    if (m_mode == Mode::LivePreview && !isCursorLine) {
        for (const DecoratedRange &dr : m_decoratedRanges) {
            if (dr.type == DecoratedRange::Table &&
                blockNum >= dr.firstBlock && blockNum <= dr.lastBlock) {
                QTextCharFormat transparentFmt;
                transparentFmt.setForeground(Qt::transparent);
                setFormat(0, text.length(), transparentFmt);
                return; // skip all other formatting for this block
            }
        }
    }

    // Syntax highlighting for code block content
    for (const DecoratedRange &dr : m_decoratedRanges) {
        if (dr.type == DecoratedRange::CodeBlock &&
            blockNum > dr.firstBlock && blockNum < dr.lastBlock) {
            // This is a content line inside a code block
            highlightCodeBlock(text, dr, blockNum);
            break;
        }
    }

    // Callout first line: hide the [!type] marker and style the title.
    // Works like bold — delimiters hidden, content styled. Click on title
    // reveals the [!type] prefix (same shift behavior as ** for bold).
    if (m_mode == Mode::LivePreview) {
        for (const DecoratedRange &dr : m_decoratedRanges) {
            if (dr.type == DecoratedRange::Callout && blockNum == dr.firstBlock) {
                static const QRegularExpression calloutMarkerRe(
                    QStringLiteral(R"(\[!(\w+)\]([+-])?\s*)"));
                auto match = calloutMarkerRe.match(text);
                if (match.hasMatch()) {
                    int markerStart = match.capturedStart();
                    int markerLen = match.capturedLength();
                    int titleStart = match.capturedEnd();
                    int titleLen = text.length() - titleStart;
                    bool hasCustomTitle = titleLen > 0;

                    if (isCursorLine) {
                        // Cursor on line: show everything raw (like bold when cursor is in it)
                        // Don't hide anything — raw markdown is visible
                    } else if (hasCustomTitle) {
                        // Has custom title: hide [!type] marker, style title
                        hideRange(markerStart, markerLen);
                        QTextCharFormat titleFmt;
                        titleFmt.setForeground(dr.calloutColor);
                        titleFmt.setFontWeight(QFont::Bold);
                        for (int i = titleStart; i < titleStart + titleLen; ++i) {
                            QTextCharFormat fmt = format(i);
                            fmt.merge(titleFmt);
                            setFormat(i, 1, fmt);
                        }
                    } else {
                        // No custom title: hide [! and ], show type name styled
                        // "[!note]" → hide "[!", show "note" styled, hide "]"
                        int typeStart = markerStart + 2; // skip "[!"
                        int typeLen = match.captured(1).length();
                        hideRange(markerStart, 2); // hide "[!"
                        QTextCharFormat typeFmt;
                        typeFmt.setForeground(dr.calloutColor);
                        typeFmt.setFontWeight(QFont::Bold);
                        // Capitalize first letter visually (can't change text,
                        // but the color makes it look intentional)
                        setFormat(typeStart, typeLen, typeFmt);
                        hideRange(typeStart + typeLen,
                                  markerLen - 2 - typeLen); // hide "]" and any +/-/space
                    }
                }
                break;
            }
        }
    }

    // Note: blockquote indentation is handled by Editor::applyBlockFormats()
    // because QSyntaxHighlighter can only set character formats, not block formats.
}

} // namespace Markoff
