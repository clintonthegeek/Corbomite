// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include <QString>
#include <QVector>

namespace Corbomite {

// AST for a parsed Obsidian-compatible global-search query.
// See docs/search-dsl-spec.md for the full grammar; the node taxonomy here
// matches the matcher classes documented at _internal.js:325169-325510:
//
//   Text     → cH (partial-match against any indexed key)
//   Phrase   → lH (whole-word on content key, exact elsewhere)
//   Regex    → aH (validated JS RegExp)
//   And      → hH (intersection of children)
//   Or       → pH (union of children)
//   Not      → mH (set complement)
//   Group    → transparent grouping wrapper (kept so toString can round-trip)
//   OpCall   → bH operator-call; payload `name` ∈ {path,file,content,tag,
//              line,block,section,task,task-todo,task-done,match-case,
//              ignore-case}; child[0] is the operand subquery.
//
// Future: PropertyCall (Phase 4b/Cluster I) for `[key]` / `[key:val]` once
// note_properties side-table lands.
struct SearchNode {
    enum class Kind {
        Text,
        Phrase,
        Regex,
        And,
        Or,
        Not,
        Group,
        OpCall,
    };

    Kind kind = Kind::Text;
    QString text;                                                // payload for Text/Phrase/Regex/OpCall(name)
    QVector<std::shared_ptr<SearchNode>> children;

    static std::shared_ptr<SearchNode> makeText(QString t);
    static std::shared_ptr<SearchNode> makePhrase(QString t);
    static std::shared_ptr<SearchNode> makeRegex(QString pattern);
    static std::shared_ptr<SearchNode> makeAnd(QVector<std::shared_ptr<SearchNode>> children);
    static std::shared_ptr<SearchNode> makeOr(QVector<std::shared_ptr<SearchNode>> children);
    static std::shared_ptr<SearchNode> makeNot(std::shared_ptr<SearchNode> child);
    static std::shared_ptr<SearchNode> makeGroup(std::shared_ptr<SearchNode> child);
    static std::shared_ptr<SearchNode> makeOpCall(QString name, std::shared_ptr<SearchNode> operand);
};

using SearchNodePtr = std::shared_ptr<SearchNode>;

} // namespace Corbomite
