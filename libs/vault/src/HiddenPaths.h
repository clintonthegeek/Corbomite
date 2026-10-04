// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite::VaultPaths {

/// Obsidian's hidden-path predicate, verbatim: walk the segments of a
/// vault-relative path leaf-first and hide the path as soon as one of them
/// starts with `.` — `Il`, `app/own/0362-Il.js:2-9`, called from the
/// reconciler at `app/own/0376-_l.js:63` (initial scan) and `:685`
/// (watcher-driven).
///
/// There is **no `configDir` parameterisation and no reserved-name list**.
/// `docs/obsidian-audit/domains/vault.md` §3 states the rule backwards
/// ("most dotfiles are hidden *except under* configDir"); the config dir is
/// hidden for the same reason `.git/` and `.trash/` are — its own leading
/// dot. Consequences that matter for a vault tree: any user dotfolder
/// (`.johns-notes/`, `.obs/`) is invisible in the explorer, to
/// `getFiles()`/`getMarkdownFiles()`, to the graph, and to link resolution.
///
/// The root path `/` is not hidden.
inline bool isHiddenPath(const QString &relPath)
{
    QString cur = relPath;
    while (!cur.isEmpty()) {
        const int slash = cur.lastIndexOf(QLatin1Char('/'));
        if (!cur.mid(slash + 1).startsWith(QLatin1Char('.'))) {
            if (slash <= 0) return false;
            cur.truncate(slash);
            continue;
        }
        return true;
    }
    return false;
}

} // namespace Corbomite::VaultPaths