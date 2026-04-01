// SPDX-License-Identifier: GPL-3.0-or-later
#include "TreeSitterParser.h"
#include "SourceSpan.h"

#include <tree_sitter/api.h>
#include <tree-sitter/tree-sitter-markdown.h>
#include <tree-sitter/tree-sitter-markdown-inline.h>

#include <QStringList>

namespace Markoff {

// ---------------------------------------------------------------------------
// UTF-8 ↔ QString char mapping
// ---------------------------------------------------------------------------

static QList<int> buildByteToCharMap(const QByteArray &utf8)
{
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
        else                 seqLen = 4;
        for (int j = 1; j < seqLen && (i + j) < utf8.size(); ++j)
            map[i + j] = charIdx;
        charIdx += (seqLen == 4) ? 2 : 1;
        i += seqLen;
    }
    map[utf8.size()] = charIdx;
    return map;
}

// ---------------------------------------------------------------------------
// TreeSitterParser
// ---------------------------------------------------------------------------

TreeSitterParser::TreeSitterParser()
{
    m_blockParser = ts_parser_new();
    ts_parser_set_language(m_blockParser, tree_sitter_markdown());

    m_inlineParser = ts_parser_new();
    ts_parser_set_language(m_inlineParser, tree_sitter_markdown_inline());
}

TreeSitterParser::~TreeSitterParser()
{
    if (m_blockTree) ts_tree_delete(m_blockTree);
    if (m_inlineTree) ts_tree_delete(m_inlineTree);
    if (m_blockParser) ts_parser_delete(m_blockParser);
    if (m_inlineParser) ts_parser_delete(m_inlineParser);
}

bool TreeSitterParser::parse(const QString &text)
{
    m_utf8 = text.toUtf8();
    m_byteToChar = buildByteToCharMap(m_utf8);

    // Phase 1: parse block structure
    if (m_blockTree) ts_tree_delete(m_blockTree);
    m_blockTree = ts_parser_parse_string(m_blockParser, nullptr,
                                          m_utf8.constData(),
                                          static_cast<uint32_t>(m_utf8.size()));
    if (!m_blockTree)
        return false;

    // Phase 2: parse inline content within inline nodes
    // Collect all `inline` node ranges from the block tree
    TSNode root = ts_tree_root_node(m_blockTree);

    // For now, do a simple full-text inline parse.
    // A proper implementation would use ts_parser_set_included_ranges()
    // to only parse inline regions. This works correctly for the highlighter
    // because we walk the inline tree for formatting info.
    if (m_inlineTree) ts_tree_delete(m_inlineTree);
    m_inlineTree = ts_parser_parse_string(m_inlineParser, nullptr,
                                           m_utf8.constData(),
                                           static_cast<uint32_t>(m_utf8.size()));

    return m_inlineTree != nullptr;
}

int TreeSitterParser::utf8ToCharOffset(int byteOffset) const
{
    if (byteOffset < 0) return 0;
    if (byteOffset >= m_byteToChar.size()) return m_byteToChar.last();
    return m_byteToChar[byteOffset];
}

// ---------------------------------------------------------------------------
// CST → SourceSpan conversion
// ---------------------------------------------------------------------------

/// Map a tree-sitter node type name to SourceSpan formatting flags
static void applyNodeType(SourceSpan &span, const char *type)
{
    // Block-level delimiters
    if (strcmp(type, "atx_h1_marker") == 0) { span.isDelimiter = true; span.isHeading = true; span.headingLevel = 1; }
    else if (strcmp(type, "atx_h2_marker") == 0) { span.isDelimiter = true; span.isHeading = true; span.headingLevel = 2; }
    else if (strcmp(type, "atx_h3_marker") == 0) { span.isDelimiter = true; span.isHeading = true; span.headingLevel = 3; }
    else if (strcmp(type, "atx_h4_marker") == 0) { span.isDelimiter = true; span.isHeading = true; span.headingLevel = 4; }
    else if (strcmp(type, "atx_h5_marker") == 0) { span.isDelimiter = true; span.isHeading = true; span.headingLevel = 5; }
    else if (strcmp(type, "atx_h6_marker") == 0) { span.isDelimiter = true; span.isHeading = true; span.headingLevel = 6; }
    else if (strcmp(type, "block_quote_marker") == 0) { span.isDelimiter = true; span.isBlockquoteMarker = true; }
    else if (strcmp(type, "fenced_code_block_delimiter") == 0 || strcmp(type, "code_fence_content") == 0) { span.isCodeBlockFence = true; span.isDelimiter = true; }
    else if (strcmp(type, "thematic_break") == 0) { span.isHorizontalRule = true; }
    else if (strcmp(type, "minus_metadata") == 0 || strcmp(type, "plus_metadata") == 0) { span.isFrontmatter = true; }
    else if (strcmp(type, "list_marker_dot") == 0 || strcmp(type, "list_marker_minus") == 0 ||
             strcmp(type, "list_marker_plus") == 0 || strcmp(type, "list_marker_star") == 0 ||
             strcmp(type, "list_marker_parenthesis") == 0) { span.isListMarker = true; }

    // Inline delimiters
    else if (strcmp(type, "emphasis_delimiter") == 0) { span.isDelimiter = true; }
    else if (strcmp(type, "code_span_delimiter") == 0) { span.isDelimiter = true; span.code = true; }
    else if (strcmp(type, "strikethrough_delimiter") == 0) { span.isDelimiter = true; span.strikethrough = true; }
    else if (strcmp(type, "latex_span_delimiter") == 0 || strcmp(type, "latex_block_delimiter") == 0)
        { span.isDelimiter = true; span.math = true; }

    // Inline content (non-delimiter)
    else if (strcmp(type, "emphasis") == 0) { span.italic = true; }
    else if (strcmp(type, "strong_emphasis") == 0) { span.bold = true; }
    else if (strcmp(type, "code_span") == 0) { span.code = true; }
    else if (strcmp(type, "strikethrough") == 0) { span.strikethrough = true; }
    else if (strcmp(type, "latex_span") == 0) { span.math = true; }
    else if (strcmp(type, "latex_block") == 0) { span.math = true; span.mathDisplay = true; }
    else if (strcmp(type, "wiki_link") == 0) { span.isWikilink = true; }
    else if (strcmp(type, "link") == 0 || strcmp(type, "uri_autolink") == 0) { span.isLink = true; }
    else if (strcmp(type, "image") == 0) { span.isImage = true; }
    else if (strcmp(type, "tag") == 0) { span.isTag = true; }

    // Heading content inherits heading level from parent
    else if (strcmp(type, "atx_heading") == 0) { span.isHeading = true; }
    else if (strcmp(type, "setext_heading") == 0) { span.isHeading = true; }
}

/// Determine heading level from an atx_heading node
static int headingLevelFromNode(TSNode node)
{
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_child(node, i);
        const char *type = ts_node_type(child);
        if (strncmp(type, "atx_h", 5) == 0 && strstr(type, "_marker"))
            return type[5] - '0';  // "atx_h2_marker" → 2
    }
    return 1;
}

void TreeSitterParser::walkNode(TSNode node, QList<SourceSpan> &spans) const
{
    const char *type = ts_node_type(node);
    uint32_t startByte = ts_node_start_byte(node);
    uint32_t endByte = ts_node_end_byte(node);
    uint32_t childCount = ts_node_child_count(node);
    bool isNamed = ts_node_is_named(node);

    // Leaf nodes (no children) → emit a span
    if (childCount == 0) {
        SourceSpan span;
        span.utf8Offset = static_cast<int>(startByte);
        span.utf8Length = static_cast<int>(endByte - startByte);
        span.charOffset = utf8ToCharOffset(startByte);
        span.charLength = utf8ToCharOffset(endByte) - span.charOffset;

        applyNodeType(span, type);

        // Inherit formatting from parent context
        // (handled by the caller propagating parent formatting)

        if (span.charLength > 0)
            spans.append(span);
        return;
    }

    // For container nodes, propagate formatting context to children
    // First, check if this node adds formatting
    SourceSpan parentFmt;
    applyNodeType(parentFmt, type);

    // Determine heading level for heading nodes
    int headingLevel = 0;
    if (parentFmt.isHeading && strcmp(type, "atx_heading") == 0)
        headingLevel = headingLevelFromNode(node);

    // Walk children
    for (uint32_t i = 0; i < childCount; ++i) {
        TSNode child = ts_node_child(node, i);
        walkNode(child, spans);
    }

    // After walking children, propagate parent formatting to child spans
    if (parentFmt.bold || parentFmt.italic || parentFmt.strikethrough ||
        parentFmt.code || parentFmt.math || parentFmt.isLink ||
        parentFmt.isWikilink || parentFmt.isImage || parentFmt.isHeading) {
        // Find spans within this node's range and add parent formatting
        for (int i = spans.size() - 1; i >= 0; --i) {
            SourceSpan &s = spans[i];
            if (s.utf8Offset < static_cast<int>(startByte))
                break;
            if (s.utf8Offset >= static_cast<int>(endByte))
                continue;

            if (parentFmt.bold) s.bold = true;
            if (parentFmt.italic) s.italic = true;
            if (parentFmt.strikethrough) s.strikethrough = true;
            if (parentFmt.code) s.code = true;
            if (parentFmt.math) s.math = true;
            if (parentFmt.mathDisplay) s.mathDisplay = true;
            if (parentFmt.isLink) s.isLink = true;
            if (parentFmt.isWikilink) s.isWikilink = true;
            if (parentFmt.isImage) s.isImage = true;
            if (parentFmt.isHeading) {
                s.isHeading = true;
                if (headingLevel > 0)
                    s.headingLevel = headingLevel;
            }
        }
    }
}

QList<SourceSpan> TreeSitterParser::buildSpanMap() const
{
    QList<SourceSpan> spans;

    if (!m_blockTree)
        return spans;

    // Walk the block tree for block-level structure
    TSNode blockRoot = ts_tree_root_node(m_blockTree);
    walkNode(blockRoot, spans);

    // Walk the inline tree for inline formatting
    if (m_inlineTree) {
        TSNode inlineRoot = ts_tree_root_node(m_inlineTree);
        QList<SourceSpan> inlineSpans;
        walkNode(inlineRoot, inlineSpans);

        // Merge inline spans — they provide finer-grained formatting
        // for emphasis delimiters, code span delimiters, etc.
        for (const SourceSpan &is : inlineSpans) {
            // Only add inline spans that aren't already covered by block spans
            bool covered = false;
            for (const SourceSpan &bs : spans) {
                if (bs.utf8Offset == is.utf8Offset && bs.utf8Length == is.utf8Length) {
                    covered = true;
                    break;
                }
            }
            if (!covered)
                spans.append(is);
        }
    }

    // Sort by offset
    std::sort(spans.begin(), spans.end(),
              [](const SourceSpan &a, const SourceSpan &b) {
                  return a.utf8Offset < b.utf8Offset;
              });

    return spans;
}

} // namespace Markoff
