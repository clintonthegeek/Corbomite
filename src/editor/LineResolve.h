// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <QString>
#include <markoff/core/CursorPos.h>
#include <markoff/core/MarkoffDocument.h>

namespace Corbomite::LineResolve {

/// One contract-v2 flat visual line resolved against the document.
/// The line space is MarkdownView::cursorPosition()'s (normative,
/// markoff contract-v2 spec §3): each block contributes
/// 1 + count('\n' in blockText) lines, 1-based.
struct ResolvedLine {
    Markoff::BlockId blockId;
    int blockRow = -1;             ///< index in iterateBlocks() order
    int lineStartCharInBlock = 0;  ///< UTF-16 offset of line start within blockText-as-QString
    QString lineText;              ///< the line, without trailing '\n'
};

/// nullopt when doc is null, line < 1, or past the last line.
std::optional<ResolvedLine> resolveLine(const Markoff::MarkoffDocument *doc, int line);

/// UTF-16 char offset within a block's text (as QString) → UTF-8 byte
/// offset into the block buffer. Clamps charPos to [0, length].
uint32_t byteOffsetForChar(const QString &blockText, int charPos);

/// Global UTF-8 byte offset of the (line, column) caret in the coordinate
/// space MarkoffDocument::applyFlatEdit indexes: the no-separator
/// concatenation of blockText(id) over iterateBlocks() order. `column` is
/// 1-based and clamped to the resolved line's length. nullopt when the line
/// cannot be resolved (see resolveLine). This is the bridge a flat,
/// structural insert-at-cursor (e.g. template insertion) uses.
std::optional<uint32_t> globalByteOffsetForCursor(
    const Markoff::MarkoffDocument *doc, int line, int column);

/// Resulting flat-line caret after inserting text at a caret. `origin` is the
/// pre-insert (line, column); `insertedBeforeCaret` is the text inserted up to
/// the desired caret point (cursor marker already removed). Pure flat-line
/// arithmetic: descend by the newlines in `insertedBeforeCaret`; on a fresh
/// line the column restarts from that line's content.
Markoff::CursorPos caretAfterFlatInsert(Markoff::CursorPos origin,
                                        const QString &insertedBeforeCaret);

} // namespace Corbomite::LineResolve
