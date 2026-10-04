// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/NoteMeta.h"
#include "corbomite/storage/IgnoreFilter.h"

#include <QVector>
#include <QString>

// Moved from libs/storage/ into libs/vault/ during Cluster Q.0 Phase 2 Task
// 2.1; header path is now corbomite/vault/VaultScanner.h. Namespace remains
// Corbomite:: — no source changes at callers other than the #include path.

namespace Corbomite {

class VaultScanner {
public:
    QVector<NoteMeta> scan(const QString &vaultRoot) const;

    /// Install a user-supplied path filter (from `userIgnoreFilters` in
    /// `app.json`). Matching paths are omitted from scan results.
    ///
    /// ⚠ **Scope.** In Obsidian `userIgnoreFilters` gates *search results,
    /// graph nodes and property-edit candidate lists only*
    /// (`app/own/1951-e3.js:77`, `1851-h0.js:162`, `1312-hR.js:509,538`).
    /// `computeFileMetadataAsync` (`app/own/1306-eR.js:442`) never consults
    /// it, so ignored `.md` files are still parsed, still enter
    /// `resolvedLinks`/`unresolvedLinks`, and still participate in
    /// rename-with-refactor; file-tree visibility is the separate
    /// dot-segment predicate (`VaultPaths::isHiddenPath`). Wiring this filter
    /// into an indexer or a tree build is a divergence, not a feature.
    ///
    /// It is set here anyway because **this class has zero production
    /// callers** — it is a test-only island (`docs/punch-list.md` §B,
    /// `Added 2026-10-01`), so no behaviour is wrong today. Wiring it must
    /// decide first whether the consumer is a search/graph candidate list
    /// (filter applies) or the metadata index / file tree (filter must not
    /// apply), and `Vault`/`Watcher` are the wrong place to find out.
    void setIgnoreFilter(IgnoreFilter filter) { m_ignore = std::move(filter); }

private:
    bool shouldExcludeDir(const QString &dirName) const;
    bool isNoteFile(const QString &suffix) const;

    IgnoreFilter m_ignore;
};

} // namespace Corbomite
