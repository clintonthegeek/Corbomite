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
//   start, end : UTF-16 char offsets WITHIN lineText (line-relative)
//                bracketing the text the suggester will replace on accept.
//                end is the cursor position.
//   replaceEnd : optional replacement-range end (>= end); -1 means "same
//                as end". Lets wiki-link consume a pre-existing "]]"
//                after the cursor instead of producing "]]]]".
//   query      : lineText.mid(start, end - start) — what the popup's
//                fuzzy proxy will be fed (via EditorSuggestionSet::filter).
struct EditorSuggestTriggerInfo {
    int start = -1;
    int end = -1;
    int replaceEnd = -1;
    QString query;
};

// One completion candidate. insertText is the FULL literal replacement
// for [start, replaceEnd) — closing punctuation included; there is no
// post-selection transform step (selectSuggestion is retired).
struct EditorSuggestItem {
    QString display;       // shown in the popup (also what fuzzy filters)
    QString insertText;    // literal replacement text
    QString detail;        // optional context (path, target note); may be empty
};

// The candidate UNIVERSE for the current trigger mode plus the string the
// popup's fuzzy proxy should filter by. The split matters for sub-target
// modes: in `[[Note#se` the universe is *headings of Note* and the filter
// is `se` — the popup must never fuzzy-match `Note#se` against headings.
struct EditorSuggestionSet {
    QList<EditorSuggestItem> items;
    QString filter;
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

    // Produce the candidate universe for the given trigger context. The
    // popup's CompletionFilterProxy does the fuzzy filtering/ranking
    // against set.filter — implementations return ALL mode-appropriate
    // candidates and do NOT pre-filter.
    virtual EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) = 0;
};

} // namespace Corbomite
