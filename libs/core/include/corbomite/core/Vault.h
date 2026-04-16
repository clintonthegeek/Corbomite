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
/// the vault. Writes are atomic via QSaveFile. No signals, no caching; the
/// host-side VaultModel + MetadataCache continue to own those concerns.
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
