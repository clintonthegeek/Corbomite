// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite::PathUtils {

/// Emits an `obsidian://open?vault=<vault>&file=<path>` URL.
///
/// `vaultName` is typically the vault's folder basename. `subpath` is an
/// optional `#heading` or `#^block` suffix that becomes part of the
/// `file=` query value (percent-encoded as a unit). Empty subpath omits
/// the `#...` portion.
QString obsidianUrlFor(const QString &vaultName,
                       const QString &relativePath,
                       const QString &subpath = QString());

/// Corbomite-native variant (same shape, `corbomite://` scheme).
///
/// Used alongside `obsidianUrlFor` for interop: users pasting into
/// Corbomite get direct open; Obsidian users paste the obsidian:// one.
QString corbomiteUrlFor(const QString &vaultName,
                        const QString &relativePath,
                        const QString &subpath = QString());

/// Returns a stable, vault-unique identifier string of the form
/// `<basename>-<12-char-sha256-prefix>`.  Two vaults with the same basename
/// but different absolute paths produce different ids.  The id is suitable for
/// use as a sub-directory name under AppLocalDataLocation.
///
/// Returns an empty string if `vaultRoot` is empty.
QString vaultId(const QString &vaultRoot);

/// Returns the per-vault subdirectory that Corbomite uses for regenerable
/// cache files (search index, metadata cache).  The path is NEVER inside the
/// vault and is NEVER empty (as long as `vaultRoot` is non-empty):
///
///   * Primary:   `<AppLocalDataLocation>/index/<vault-id>/`
///   * Fallback:  `<TempLocation>/corbomite/index/<vault-id>/`
///               (used only when AppLocalDataLocation is unavailable)
///
/// Returns an empty string only if `vaultRoot` itself is empty.
/// The caller is responsible for creating the directory (QDir::mkpath).
QString vaultLocalDataDir(const QString &vaultRoot);

}  // namespace Corbomite::PathUtils
