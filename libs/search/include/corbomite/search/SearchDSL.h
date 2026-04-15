// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include "corbomite/search/SearchAst.h"

namespace Corbomite::SearchDSL {

struct ParseResult {
    SearchNodePtr root;     // null on empty/whitespace query (Obsidian "match nothing")
    QString error;          // empty on success
    int errorOffset = -1;   // 0-based char offset of failing token, -1 on success
};

// Parse an Obsidian-compatible global-search query.
//
// Grammar / behaviour reference: docs/search-dsl-spec.md.
// Empty or whitespace-only input yields a successful result with null root —
// callers should treat as "no structural query, fall back to plain FTS".
//
// Errors surfaced: unrecognised operator, exclusive-nesting violation,
// tag-operand-not-text, invalid regex. Other malformed input is silently
// tolerated (trailing OR, unterminated quote/regex, dangling colon) per
// Obsidian's parser quirks.
ParseResult parse(const QString &query);

// Operators currently recognised (Phase 4a subset). Future operators that need
// markdown-AST or new schema (line/block/section/task*/property) are accepted
// by the parser as OpCall nodes; the planner will reject them until support
// lands. Set is exposed for the SearchPanel "?" helper popover.
const QStringList &supportedOperators();

} // namespace Corbomite::SearchDSL
