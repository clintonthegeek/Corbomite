// SPDX-License-Identifier: GPL-3.0-or-later
#include "SourceSpan.h"
#include "DocumentBuilder_p.h"

namespace Markoff {

// ---------------------------------------------------------------------------
// UTF-8 → QString offset mapping
// ---------------------------------------------------------------------------

QList<int> buildUtf8ToCharMap(const QByteArray &utf8)
{
    // For each byte position in the UTF-8 string, compute the corresponding
    // QString (UTF-16) character index. Multi-byte UTF-8 sequences map
    // multiple bytes to the same char index.
    QList<int> map(utf8.size() + 1, 0);

    int charIdx = 0;
    int i = 0;
    while (i < utf8.size()) {
        map[i] = charIdx;
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        int seqLen;
        if (c < 0x80)       seqLen = 1;
        else if (c < 0xE0)  seqLen = 2;
        else if (c < 0xF0)  seqLen = 3;
        else                 seqLen = 4;  // 4-byte → surrogate pair in UTF-16

        // All bytes in the sequence map to the same char index
        for (int j = 1; j < seqLen && (i + j) < utf8.size(); ++j)
            map[i + j] = charIdx;

        charIdx += (seqLen == 4) ? 2 : 1;  // surrogate pair = 2 UTF-16 units
        i += seqLen;
    }
    map[utf8.size()] = charIdx;  // sentinel for end-of-string

    return map;
}

// ---------------------------------------------------------------------------
// Span map builder
// ---------------------------------------------------------------------------

/// Create a delimiter span for bytes between prevEnd and currentStart
static SourceSpan makeDelimiterSpan(int offset, int length,
                                     const InlineRun &context, bool blockLevel = false)
{
    SourceSpan s;
    s.utf8Offset = offset;
    s.utf8Length = length;
    s.isDelimiter = true;
    // Carry the formatting context so the delimiter can be styled in source mode
    s.bold = context.bold;
    s.italic = context.italic;
    s.code = context.code;
    return s;
}

static SourceSpan makeContentSpan(const InlineRun &run)
{
    SourceSpan s;
    s.utf8Offset = run.sourceOffset;
    s.utf8Length = run.sourceLength;
    s.bold = run.bold;
    s.italic = run.italic;
    s.strikethrough = run.strikethrough;
    s.code = run.code;
    s.math = run.math;
    s.mathDisplay = run.mathDisplay;
    s.highlight = run.highlight;
    s.comment = run.comment;
    s.isTag = run.isTag;
    s.isLink = !run.linkHref.isEmpty();
    s.isWikilink = !run.wikiTarget.isEmpty();
    s.isImage = !run.imageSrc.isEmpty();
    return s;
}

static void buildSpansForBlock(const Block &block, QList<SourceSpan> &spans,
                                const QByteArray &utf8Source)
{
    // Block-level span for special block types
    if (block.type == MD_BLOCK_HR) {
        SourceSpan s;
        s.utf8Offset = block.sourceOffset;
        s.utf8Length = block.sourceLength;
        s.isHorizontalRule = true;
        spans.append(s);
        return;
    }

    // For blocks with inline content, emit spans for each InlineRun.
    // The gaps between runs are delimiters (**, *, `, [, ], etc.)
    if (!block.inlines.isEmpty()) {
        // Block-level formatting context
        bool isHeading = (block.type == MD_BLOCK_H);
        int headingLevel = block.headingLevel;

        for (int i = 0; i < block.inlines.size(); ++i) {
            const InlineRun &run = block.inlines[i];

            // Gap before this run = delimiter
            int gapStart;
            if (i == 0) {
                gapStart = block.sourceOffset;
            } else {
                const InlineRun &prev = block.inlines[i - 1];
                gapStart = prev.sourceOffset + prev.sourceLength;
            }

            if (gapStart >= 0 && gapStart < run.sourceOffset) {
                SourceSpan delim = makeDelimiterSpan(gapStart, run.sourceOffset - gapStart, run);
                if (isHeading && i == 0) {
                    delim.isHeading = true;
                    delim.headingLevel = headingLevel;
                }
                spans.append(delim);
            }

            // The content run itself
            SourceSpan content = makeContentSpan(run);
            if (isHeading) {
                content.isHeading = true;
                content.headingLevel = headingLevel;
            }
            spans.append(content);
        }

        // Gap after last run = trailing delimiter
        if (!block.inlines.isEmpty()) {
            const InlineRun &last = block.inlines.last();
            int afterLast = last.sourceOffset + last.sourceLength;
            int blockEnd = block.sourceOffset + block.sourceLength;
            if (afterLast < blockEnd) {
                SourceSpan delim = makeDelimiterSpan(afterLast, blockEnd - afterLast, last);
                spans.append(delim);
            }
        }
    }

    // Recurse into children
    for (const Block &child : block.children)
        buildSpansForBlock(child, spans, utf8Source);
}

QList<SourceSpan> buildSpanMap(const QList<Block> &blocks,
                                const QByteArray &utf8Source)
{
    QList<SourceSpan> spans;
    spans.reserve(blocks.size() * 4);  // rough estimate

    for (const Block &block : blocks)
        buildSpansForBlock(block, spans, utf8Source);

    // Sort by offset for efficient lookup
    std::sort(spans.begin(), spans.end(),
              [](const SourceSpan &a, const SourceSpan &b) {
                  return a.utf8Offset < b.utf8Offset;
              });

    // Compute char offsets from utf8 offsets
    QList<int> charMap = buildUtf8ToCharMap(utf8Source);
    for (SourceSpan &s : spans) {
        if (s.utf8Offset >= 0 && s.utf8Offset < charMap.size())
            s.charOffset = charMap[s.utf8Offset];
        int endByte = s.utf8Offset + s.utf8Length;
        if (endByte >= 0 && endByte < charMap.size())
            s.charLength = charMap[endByte] - s.charOffset;
        else if (endByte == charMap.size() - 1)
            s.charLength = charMap.last() - s.charOffset;
    }

    return spans;
}

} // namespace Markoff
