// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/EditorSuggestManager.h"

namespace Corbomite {

EditorSuggestManager::EditorSuggestManager(QObject *parent)
    : QObject(parent)
{
}

void EditorSuggestManager::registerSuggest(EditorSuggest *suggester)
{
    if (!suggester || m_suggesters.contains(suggester)) return;
    m_suggesters.append(suggester);
}

void EditorSuggestManager::unregisterSuggest(EditorSuggest *suggester)
{
    m_suggesters.removeAll(suggester);
}

std::optional<EditorSuggestManager::DispatchResult>
EditorSuggestManager::dispatch(int cursorPos,
                                 const QString &lineText,
                                 NoteDocument *file) const
{
    // Defensive clamp (punch-list P3): rapid-edit races can hand us a
    // cursorPos one past the line end; suggesters slice lineText with it.
    cursorPos = qBound(0, cursorPos, int(lineText.length()));
    for (EditorSuggest *s : m_suggesters) {
        auto trig = s->onTrigger(cursorPos, lineText, file);
        if (trig) return DispatchResult{s, *trig};
    }
    return std::nullopt;
}

} // namespace Corbomite
