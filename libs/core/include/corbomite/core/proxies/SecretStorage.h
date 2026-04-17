// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Corbomite {

/// Secret-storage facade for plugins with the "secrets" permission.
///
/// Backends (in priority order):
///   1. Platform keyring via QtKeychain (KWallet / GNOME Keyring /
///      macOS Keychain / Windows Credential Manager) — persists across
///      restarts, compiled in when CORBOMITE_HAVE_KEYRING is defined.
///   2. In-process QHash fallback — used when QtKeychain is unavailable
///      at build time, and at runtime when a keyring operation fails
///      (e.g. no running secret service). Does not persist; a qCWarning
///      is emitted on first runtime fallback.
///
/// Keys are namespaced internally as `<pluginId>.<id>` so plugins cannot
/// collide on the same key in shared backends (both QtKeychain and the
/// process-wide QHash fallback).
///
/// All operations short-circuit to failure/empty when the "secrets"
/// permission is not in the granted set. The permission-gated constructor
/// form takes the granted set; the legacy single-arg form assumes granted
/// (kept for tests and internal wiring during Cluster Q migration).
class SecretStorage
{
public:
    explicit SecretStorage(QString pluginId);
    SecretStorage(QString pluginId, QSet<QString> granted);

    SecretStorage(const SecretStorage &) = delete;
    SecretStorage &operator=(const SecretStorage &) = delete;

    bool setSecret(const QString &id, const QString &value);
    QString getSecret(const QString &id) const;
    bool deleteSecret(const QString &id);

    /// Returns *local* ids (without the plugin prefix), sorted.
    /// QtKeychain does not expose an enumeration API, so this only reflects
    /// ids observed via the in-process fallback or written during this
    /// session. It remains useful for test introspection and for plugins
    /// that track their own key list.
    QStringList listSecrets() const;

    const QString &pluginId() const { return m_pluginId; }

private:
    bool hasSecretsPermission() const;

    QString        m_pluginId;
    QSet<QString>  m_granted;
};

} // namespace Corbomite
