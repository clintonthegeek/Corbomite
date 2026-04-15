// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Corbomite {

/// Path filter matching Obsidian's `userIgnoreFilters` semantics
/// (see `docs/obsidian-audit/domains/metadata.md §3` "Inputs consulted").
///
/// Accepts a list of patterns. Each pattern is one of two shapes:
///
///   - Plain prefix: `tmp/` / `drafts/work.md` — anchored at path start,
///     the literal string (with regex metachars escaped). Matches any path
///     whose prefix equals the pattern.
///   - Regex: `/regex-body/` — the text between the slashes is compiled
///     as a regular expression. Invalid regexes are skipped with a console
///     warning in Obsidian; this port silently skips them.
///
/// A path is ignored if any pattern matches. `matches(path)` returns true
/// when the path should be excluded from scans / metadata / file explorer.
class IgnoreFilter
{
public:
    IgnoreFilter() = default;

    /// Construct from a list of user-supplied patterns.
    static IgnoreFilter fromPatterns(const QStringList &patterns);

    /// True if this filter should exclude `vaultRelativePath`.
    bool matches(const QString &vaultRelativePath) const;

    /// Number of compiled patterns (after invalid-regex filtering).
    int patternCount() const { return m_patterns.size(); }

private:
    struct Pattern {
        QRegularExpression re;
        bool isRegex = false;
        QString original;
    };
    QVector<Pattern> m_patterns;
};

} // namespace Corbomite
