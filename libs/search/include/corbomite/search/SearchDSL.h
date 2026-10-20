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

// Compiled, executor-ready form of a parsed query. Coverage:
//   - bare text / quoted phrase / OR / AND       → fts5Query
//   - path: / file: / content:                   → fts5Query (column qualified)
//   - tag:#X                                     → requiredTags / excludedTags
//   - match-case:                                → caseSensitiveTerms + inner query
//   - /regex/                                    → regexPatterns
//   - everything else (line/block/section/task*/`[key]`/`[key: value]`/
//     TRUE/FALSE/EMPTY/`>`/`<`)                  → accumulated into `unsupported`
//     for the UI to surface; they do not filter results.
//
// Empty `fts5Query` AND empty tag lists ⇒ "match nothing" (an empty
// CompiledPlan is the result of an empty query).
//
// KNOWN GAP: Obsidian's bare `Text` term is a *substring* regex (app/own/1166-ZA.js:116,
// built by `jd`), so `foo` matches `barfoo`. A quoted `Phrase` is whole-word on the
// `content` key and a plain substring on filename/filepath/tag
// (app/own/1166-ZA.js:90,99). FTS5 matches whole tokens, so today both compile to
// the same quoted token and the substring half of that distinction is unreachable.
// Closing it means the executor must stop gating recall on `notes_fts MATCH ?`
// (libs/storage/src/SQLiteIndex.cpp) — a post-filter can only narrow results, not
// widen them.
struct CompiledPlan {
    QString fts5Query;
    // FTS5 fragment matching notes that must be *excluded* from the result.
    // Populated when the parsed query has no positive sibling for a NOT term
    // (e.g. top-level `-foo`, or `-foo -bar`). FTS5 MATCH cannot express a
    // bare top-level negation, so the executor applies this as a NOT IN
    // sub-query against a full-table or fts5Query-restricted scan.
    QString excludedFts5Query;
    QStringList requiredTags;
    QStringList excludedTags;
    // Post-filter predicates applied to FTS candidates' content:
    //   - every regex in regexPatterns must match (case-insensitive by default)
    //   - every term in caseSensitiveTerms must appear with Qt::CaseSensitive
    // Populated by `/regex/` and `match-case:` operators.
    QStringList regexPatterns;
    QStringList caseSensitiveTerms;
    QStringList unsupported;
};

CompiledPlan compile(const SearchNodePtr &root);

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

// Operators currently recognised. Future operators that need
// markdown-AST or new schema (line/block/section/task*/property) are accepted
// by the parser as OpCall nodes; the planner will reject them until support
// lands. Set is exposed for the SearchPanel "?" helper popover.
const QStringList &supportedOperators();

} // namespace Corbomite::SearchDSL
