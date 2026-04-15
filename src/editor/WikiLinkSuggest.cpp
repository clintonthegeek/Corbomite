// SPDX-License-Identifier: GPL-3.0-or-later
#include "WikiLinkSuggest.h"

#include "corbomite/core/NoteMeta.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/search/FuzzyMatcher.h"

namespace Corbomite {

WikiLinkSuggest::WikiLinkSuggest(VaultModel *vault)
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

QStringList WikiLinkSuggest::getSuggestions(const EditorSuggestTriggerInfo &ctx)
{
    if (!m_vault) return {};
    QStringList names;
    const auto notes = m_vault->allNotes();
    names.reserve(notes.size());
    for (const auto &n : notes) {
        QString name = n.relativePath;
        const int slash = name.lastIndexOf(QLatin1Char('/'));
        if (slash >= 0) name = name.mid(slash + 1);
        const int dot = name.lastIndexOf(QLatin1Char('.'));
        if (dot > 0) name = name.left(dot);
        names.append(name);
    }
    if (ctx.query.isEmpty()) return names;
    auto prepared = FuzzyMatcher::prepareQuery(ctx.query);
    QStringList ranked;
    for (const QString &n : names) {
        if (FuzzyMatcher::fuzzySearch(prepared, n).has_value()) ranked.append(n);
    }
    return ranked;
}

QString WikiLinkSuggest::selectSuggestion(const QString &chosen,
                                            const EditorSuggestTriggerInfo &)
{
    return chosen + QStringLiteral("]]");
}

} // namespace Corbomite
