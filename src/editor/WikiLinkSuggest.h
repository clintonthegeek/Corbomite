// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/EditorSuggest.h"

namespace Corbomite {

class Vault;

// Built-in suggester for `[[wiki-link]]` completion. Activates when the
// user types `[[` and continues until the cursor leaves the link context
// (closing `]`, newline, or moving outside the trigger range).
class WikiLinkSuggest : public EditorSuggest {
public:
    explicit WikiLinkSuggest(Vault *vault);

    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *file) override;
    QStringList getSuggestions(const EditorSuggestTriggerInfo &ctx) override;
    QString selectSuggestion(const QString &chosen,
                              const EditorSuggestTriggerInfo &ctx) override;

    void setVault(Vault *vault) { m_vault = vault; }

private:
    Vault *m_vault;
};

} // namespace Corbomite
