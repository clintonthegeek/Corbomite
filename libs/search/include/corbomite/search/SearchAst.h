// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include <QString>
#include <QVector>

namespace Corbomite {

// AST for a parsed Obsidian-compatible global-search query.
//
// The node taxonomy mirrors the matcher classes of app/own/1163-UA.js and
// app/own/1166-ZA.js, as dispatched by the parser at app/own/1170-hP.js:
//
//   Text           → tP  (`jd`-escaped substring regex)
//   Phrase         → eP  (whole-word on the `content` key, substring elsewhere)
//   Regex          → ZA  (validated JS RegExp)
//   And            → iP  (intersection)
//   Or             → rP  (union)
//   Not            → sP  (set complement)
//   Group          → paren grouping, kept so toString can round-trip. Obsidian
//                    has no Group node — it returns the inner node verbatim
//                    (1170-hP.js:52-64) — so this is Corbomite-only and must not
//                    be given match semantics.
//   OpCall         → bH operator-call; payload `text` is the LOWERCASED name from
//                    the table at app/own/1175-SP.js:148-221 — match-case,
//                    ignore-case, path, file, content, line, block, section,
//                    task, task-todo, task-done, tag. children[0] is the operand.
//   PropertyCall   → JA  `[key]` / `[key: value]`. children[0] is the key AST,
//                    children[1] the value AST. A missing child[1] is an
//                    existence check. Key matching is case-insensitive equality
//                    against the frontmatter key name (1166-ZA.js:102-104); the
//                    value matches as a substring regex element-wise over
//                    arrays (1166-ZA.js:44-56).
//   PropertyValue  → _A  the TRUE / FALSE / EMPTY keywords. Payload `text` is
//                    the keyword. It compares a *frontmatter property value* to
//                    true/false/null respectively, so at top level EMPTY matches
//                    every file and TRUE/FALSE match nothing (1163-UA.js:21-28).
//   Compare        → aP  bare `>` / `<`. Payload `text` is the operator; the
//                    operand is a Text or Phrase child and is compared as a JS
//                    relational (1166-ZA.js:190-206).
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
        PropertyCall,
        PropertyValue,
        Compare,
    };

    Kind kind = Kind::Text;
    QString text;                                                // payload for Text/Phrase/Regex/OpCall/PropertyValue/Compare
    QVector<std::shared_ptr<SearchNode>> children;

    static std::shared_ptr<SearchNode> makeText(QString t);
    static std::shared_ptr<SearchNode> makePhrase(QString t);
    static std::shared_ptr<SearchNode> makeRegex(QString pattern);
    static std::shared_ptr<SearchNode> makeAnd(QVector<std::shared_ptr<SearchNode>> children);
    static std::shared_ptr<SearchNode> makeOr(QVector<std::shared_ptr<SearchNode>> children);
    static std::shared_ptr<SearchNode> makeNot(std::shared_ptr<SearchNode> child);
    static std::shared_ptr<SearchNode> makeGroup(std::shared_ptr<SearchNode> child);
    static std::shared_ptr<SearchNode> makeOpCall(QString name, std::shared_ptr<SearchNode> operand);
    static std::shared_ptr<SearchNode> makeProperty(std::shared_ptr<SearchNode> key,
                                                    std::shared_ptr<SearchNode> value);
    static std::shared_ptr<SearchNode> makePropertyValue(QString keyword);
    static std::shared_ptr<SearchNode> makeCompare(QString op, std::shared_ptr<SearchNode> operand);
};

using SearchNodePtr = std::shared_ptr<SearchNode>;

} // namespace Corbomite
