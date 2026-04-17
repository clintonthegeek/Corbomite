// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class Vault;
class TFolder;

/// Abstract base for `TFile` and `TFolder`. Non-QObject, cheap to allocate.
/// Owned by `Vault` (via `QHash<QString, std::unique_ptr<TAbstractFile>>`);
/// consumers hold non-owning raw pointers.
///
/// Tombstone on delete: `deleted = true` is set before the `unique_ptr` is
/// drained, so subscribers holding the pointer and receiving the
/// `Vault::deletedFile` signal can observe the flag and react safely.
class TAbstractFile
{
public:
    QString  path;                  ///< NFC-normalized, /-separated, root-relative.
    QString  name;                  ///< basename(path).
    TFolder *parent = nullptr;      ///< Non-owning; Vault owns the tree.
    bool     deleted = false;       ///< Tombstone — set true on removal.

    Vault *vault() const { return m_vault; }

    /// Updates `path` and `name`. Subclasses may override to update derived
    /// fields (TFile updates basename/extension).
    virtual void setPath(const QString &newPath);

    /// Returns the path the file would have if renamed to `newName` within
    /// its current parent. Strips control chars [\x00-\x1F] and trims.
    /// Returns empty string when detached (no parent).
    QString getNewPathAfterRename(const QString &newName) const;

    virtual ~TAbstractFile() = default;

protected:
    TAbstractFile(Vault *v, QString p);

private:
    Vault *m_vault;
};

} // namespace Corbomite
