// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Corbomite {

/// Vault-root facade for plugin proxies (VaultReader / VaultWriter).
///
/// Thin, path-only wrapper over the vault root directory: all operations take
/// a vault-relative path, reject absolute paths, and clamp `..` escapes inside
/// the vault. Writes are atomic via QSaveFile. No signals, no caching.
///
/// **Open tension (see PROJECT-STATE "Open questions"):** the real vault
/// object is `Corbomite::VaultModel` in `libs/models/` — downstream of
/// `libs/core/`, so we can't reference it from here without inverting the
/// dep graph. This class was introduced in Cluster Q Task 7 (commit
/// `b9a271d`) to unblock the plugin proxy wire-up; writes through it
/// currently bypass `VaultModel` (no `noteAdded`/`noteModified`/etc.
/// signals, no `NoteDocument` cache invalidation). Reads are safe. Do
/// **not** wire a bare `Vault(vaultPath)` into `CorbomiteApp` in Cluster Q
/// Task 12 without first resolving that tension — likely by promoting
/// `Vault` to a pure-virtual interface that `VaultModel` implements.
class Vault
{
public:
    explicit Vault(const QString &rootPath);

    QString root() const { return m_root; }

    // Reads.
    QByteArray  read(const QString &relativePath) const;
    bool        exists(const QString &relativePath) const;
    QStringList list(const QString &subdir = {}) const;

    // Writes.
    bool create(const QString &relativePath, const QByteArray &body);
    bool write(const QString &relativePath, const QByteArray &body);
    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &relativePath);

private:
    /// Returns the vault-absolute path, or an empty QString if `relative` is
    /// absolute, escapes the vault via `..`, or the vault root is empty.
    QString absolutePath(const QString &relative) const;

    QString m_root;
};

} // namespace Corbomite
