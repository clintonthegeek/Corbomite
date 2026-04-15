// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class DataAdapter;

/// Vault-local trash implementation (`trashOption: "local"` mode).
///
/// Matches Obsidian's desktop convention from
/// `docs/obsidian-audit/domains/vault.md §3`:
///
///   `.trash/<basename><suffix>.<ext>`
///
/// - Suffix is `""` for the first copy of a given basename.
/// - On collision: `" 2"`, `" 3"`, … (space + number, pre-extension).
/// - Original extension is preserved.
///
/// Usage:
///   VaultTrash t(&fs, "/path/to/vault");
///   t.moveToTrash("folder/doomed.md");   // → .trash/doomed.md (or " 2" etc.)
class VaultTrash
{
public:
    VaultTrash(DataAdapter *fs, const QString &vaultRoot);

    /// Move `relativePath` (vault-relative) into the local `.trash/` dir.
    /// Returns the resolved trash path on success, empty string on failure.
    QString moveToTrash(const QString &relativePath);

    /// Absolute path to the vault's `.trash/` directory.
    QString trashDir() const;

private:
    DataAdapter *m_fs = nullptr;
    QString m_vaultRoot;
};

} // namespace Corbomite
