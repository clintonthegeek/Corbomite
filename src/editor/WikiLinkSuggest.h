// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/EditorSuggest.h"

namespace Corbomite {

class Vault;
class LinkResolver;
class MetadataCache;

// Built-in suggester for `[[wiki-link]]` completion: note names (+ aliases,
// `#heading` and `#^block` sub-targets in later phases). Activates when the
// user types `[[` and continues until the cursor leaves the link context
// (closing `]`, newline, or moving outside the trigger range). Spec:
// docs/superpowers/specs/2026-06-11-completion-revival-design.md §8.
class WikiLinkSuggest : public EditorSuggest {
public:
    explicit WikiLinkSuggest(Vault *vault);

    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *file) override;
    EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) override;

    void setVault(Vault *vault) { m_vault = vault; }
    void setLinkResolver(LinkResolver *resolver) { m_resolver = resolver; }
    void setMetadataCache(MetadataCache *cache) { m_cache = cache; }

private:
    Vault *m_vault;
    LinkResolver *m_resolver = nullptr;
    MetadataCache *m_cache = nullptr;       // used from A2 (aliases/headings)
    QString m_sourcePath;                   // relativePath of last onTrigger's file
};

} // namespace Corbomite
