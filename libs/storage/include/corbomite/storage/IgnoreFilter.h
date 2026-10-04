// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Corbomite {

/// Path filter for Obsidian's `userIgnoreFilters` config value
/// (`app/own/1306-eR.js:588`). Entries are **regular expressions, not globs**
/// — `docs/obsidian-audit/domains/vault.md §2`'s `// glob patterns`
/// annotation is wrong (see
/// `docs/obsidian-audit/addenda/2026-10-03-vault-hidden-paths-and-userignore.md`).
///
/// Each entry is trimmed first, then has one of two shapes:
///
///   - `/regex-body/` (length > 2, leading *and* trailing slash): the inner
///     text is compiled raw and **unanchored**, e.g. `/^Archive/`.
///   - Anything else: the entry is regex-escaped and anchored with `^`,
///     i.e. a **literal case-insensitive prefix** match. So `Archive*`
///     matches the literal prefix `Archive*` (the `*` is escaped), and
///     `tmp/` does not match `Notes/tmp/x.md`.
///   - An entry that is empty after trimming is dropped, as is an entry
///     whose regex fails to compile.
///
/// Both forms are matched **case-insensitively** (Obsidian passes the `i`
/// flag to both `RegExp` constructions).
///
/// Scope: `userIgnoreFilters` is a *search / graph / property-edit* gate.
/// Obsidian's `computeFileMetadataAsync` never consults it, so ignored
/// `.md` files are still parsed, still enter `resolvedLinks`, and still
/// participate in rename-with-refactor; file-tree visibility is the
/// separate dot-segment predicate. Wiring this filter into an indexer or a
/// tree build is therefore a divergence, not a feature.
class IgnoreFilter
{
public:
    IgnoreFilter() = default;

    /// Construct from a list of user-supplied `userIgnoreFilters` entries.
    static IgnoreFilter fromPatterns(const QStringList &patterns);

    /// True if this filter should exclude `vaultRelativePath` (the full
    /// vault-relative path, which is what Obsidian matches against).
    bool matches(const QString &vaultRelativePath) const;

    /// Number of compiled patterns, after dropping trimmed-empty entries and
    /// regexes that fail to compile.
    int patternCount() const { return m_patterns.size(); }

private:
    QVector<QRegularExpression> m_patterns;
};

} // namespace Corbomite
