// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "corbomite/core/Component.h"

namespace Corbomite {

// Trigger range + filter query produced by EditorSuggest::onTrigger.
// Mirrors Obsidian's EditorSuggestTriggerInfo (domains/editor.md §3).
//
//   start, end : absolute character offsets in the document (UTF-16 code
//                units) bracketing the text the suggester will replace on
//                accept.
//   query      : the substring between [start, end) — what the suggester
//                ranks its candidates against. Typically also what it
//                feeds to FuzzyMatcher.
struct EditorSuggestTriggerInfo {
    int start = -1;
    int end = -1;
    QString query;
};

class NoteDocument;

// Abstract base for in-editor suggesters (wiki-link, tag, plugin-supplied).
//
// Per docs/obsidian-audit/domains/editor.md §3 — every cursor change asks
// every registered EditorSuggest in turn whether *it* should activate; the
// first one to return a non-null TriggerInfo wins. There is no priority
// system; insertion order is the entire coordination mechanism (a load-
// bearing compat invariant — built-ins always shadow plugin overrides of
// `[[` / `#` because they register first).
//
// Inherits Component (Cluster C) so each suggester has the standard
// load/unload + child-component lifecycle.
class EditorSuggest : public Component {
public:
    ~EditorSuggest() override = default;

    // Inspect cursor context. Return TriggerInfo if this suggester wants to
    // activate, nullopt otherwise. `cursorPos` is the absolute char offset;
    // `lineText` is the text of the current logical line; `file` is the
    // open document (may be nullptr for unattached editors).
    virtual std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                                const QString &lineText,
                                                                NoteDocument *file) = 0;

    // Produce ranked candidate strings for the given trigger context.
    // Implementations typically pass `ctx.query` through Corbomite::FuzzyMatcher.
    virtual QStringList getSuggestions(const EditorSuggestTriggerInfo &ctx) = 0;

    // Convert a chosen candidate into the literal text that replaces
    // [ctx.start, ctx.end). Implementations append closing punctuation
    // (e.g. `]]` for wiki-link).
    virtual QString selectSuggestion(const QString &chosen,
                                       const EditorSuggestTriggerInfo &ctx) = 0;
};

} // namespace Corbomite
