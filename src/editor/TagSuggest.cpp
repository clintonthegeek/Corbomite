// SPDX-License-Identifier: GPL-3.0-or-later
#include "TagSuggest.h"

#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/search/FuzzyMatcher.h"

namespace Corbomite {

TagSuggest::TagSuggest(SQLiteIndex *index)
    : m_index(index)
{
}

std::optional<EditorSuggestTriggerInfo>
TagSuggest::onTrigger(int cursorPos, const QString &lineText, NoteDocument *file)
{
    Q_UNUSED(file)
    if (cursorPos <= 0 || cursorPos > lineText.length()) return std::nullopt;
    // Walk back to find a `#` whose preceding char is whitespace or
    // start-of-line. This avoids in-word matches and ATX headings (which
    // are followed by space anyway).
    int i = cursorPos - 1;
    while (i >= 0) {
        const QChar c = lineText.at(i);
        if (c.isSpace()) return std::nullopt;
        if (c == QLatin1Char('#')) {
            const bool atLineStart = (i == 0);
            const bool prevIsSpace = i > 0 && lineText.at(i - 1).isSpace();
            if (!atLineStart && !prevIsSpace) return std::nullopt;
            EditorSuggestTriggerInfo info;
            info.start = i + 1;     // after the '#'
            info.end = cursorPos;
            info.query = lineText.mid(info.start, info.end - info.start);
            return info;
        }
        --i;
    }
    return std::nullopt;
}

EditorSuggestionSet TagSuggest::getSuggestions(const EditorSuggestTriggerInfo &ctx)
{
    // Mechanical v2 conversion (Task 6): return the full candidate universe;
    // the popup's fuzzy proxy filters against set.filter. Tag insertion is
    // just the tag text — the leading '#' is already in place — so display
    // and insertText coincide. Behavioral richness lands in Task 7.
    EditorSuggestionSet set;
    set.filter = ctx.query;
    if (!m_index) return set;
    // SQLiteIndex surfaces tags with the leading '#' verbatim. Strip it so
    // suggestions match the contract the old VaultModel::allTags used.
    QStringList tags = m_index->allTags();
    for (QString &t : tags) {
        if (t.startsWith(QLatin1Char('#'))) t.remove(0, 1);
        set.items.append({t, t, {}});
    }
    return set;
}

} // namespace Corbomite
