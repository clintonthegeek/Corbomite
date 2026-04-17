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

QStringList TagSuggest::getSuggestions(const EditorSuggestTriggerInfo &ctx)
{
    if (!m_index) return {};
    // SQLiteIndex surfaces tags with the leading '#' verbatim. Strip it so
    // suggestions match the contract the old VaultModel::allTags used.
    QStringList tags = m_index->allTags();
    for (QString &t : tags) {
        if (t.startsWith(QLatin1Char('#'))) t.remove(0, 1);
    }
    if (ctx.query.isEmpty()) return tags;
    auto prepared = FuzzyMatcher::prepareQuery(ctx.query);
    QStringList ranked;
    for (const QString &t : tags) {
        if (FuzzyMatcher::fuzzySearch(prepared, t).has_value()) ranked.append(t);
    }
    return ranked;
}

QString TagSuggest::selectSuggestion(const QString &chosen,
                                      const EditorSuggestTriggerInfo &)
{
    return chosen;  // tag insertion is just the tag text; '#' is already in place
}

} // namespace Corbomite
