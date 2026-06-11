// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <QString>
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

} // namespace Corbomite::LineResolve
