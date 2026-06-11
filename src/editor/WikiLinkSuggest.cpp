// SPDX-License-Identifier: GPL-3.0-or-later
#include "WikiLinkSuggest.h"

#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/search/FuzzyMatcher.h"

namespace Corbomite {

WikiLinkSuggest::WikiLinkSuggest(Vault *vault)
    : m_vault(vault)
{
}

std::optional<EditorSuggestTriggerInfo>
WikiLinkSuggest::onTrigger(int cursorPos, const QString &lineText, NoteDocument *file)
{
    Q_UNUSED(file)
    // Walk back from the cursor (column-relative) looking for `[[`. Bail on
    // newline (caller passes a single line so this is implicit) or `]` that
    // would close the link.
    if (cursorPos < 0 || cursorPos > lineText.length()) return std::nullopt;
    int i = cursorPos - 1;
    while (i >= 1) {
        const QChar c = lineText.at(i);
        if (c == QLatin1Char(']')) return std::nullopt;
        if (c == QLatin1Char('[') && lineText.at(i - 1) == QLatin1Char('[')) {
            EditorSuggestTriggerInfo info;
            info.start = i + 1;     // after the second '['
            info.end = cursorPos;
            info.query = lineText.mid(info.start, info.end - info.start);
            return info;
        }
        --i;
    }
    return std::nullopt;
}

EditorSuggestionSet WikiLinkSuggest::getSuggestions(const EditorSuggestTriggerInfo &ctx)
{
    // Mechanical v2 conversion (Task 6): return the full candidate universe;
    // the popup's fuzzy proxy filters against set.filter. insertText carries
    // the closing `]]` that the retired selectSuggestion used to append.
    // Behavioral richness (replaceEnd, path disambiguation, sub-target
    // headings) lands in Task 7.
    EditorSuggestionSet set;
    set.filter = ctx.query;
    if (!m_vault) return set;
    const auto files = m_vault->getMarkdownFiles();
    set.items.reserve(files.size());
    for (auto *tf : files) {
        if (!tf) continue;
        set.items.append({tf->basename, tf->basename + QStringLiteral("]]"), {}});
    }
    return set;
}

} // namespace Corbomite
