// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/EditorSuggest.h"

namespace Corbomite {

class SQLiteIndex;

// Built-in suggester for `#tag` completion. Activates when the user types
// `#` (preceded by whitespace or start-of-line, to avoid matching headings
// or in-word `#`).
class TagSuggest : public EditorSuggest {
public:
    explicit TagSuggest(SQLiteIndex *index);

    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *file) override;
    QStringList getSuggestions(const EditorSuggestTriggerInfo &ctx) override;
    QString selectSuggestion(const QString &chosen,
                              const EditorSuggestTriggerInfo &ctx) override;

    void setIndex(SQLiteIndex *index) { m_index = index; }

private:
    SQLiteIndex *m_index;
};

} // namespace Corbomite
