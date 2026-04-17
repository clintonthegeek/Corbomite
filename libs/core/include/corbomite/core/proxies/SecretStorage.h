// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

/// Secret-storage facade for plugins with the "secrets" permission.
///
/// MVP backend is in-process per-plugin storage. Persistent keyring
/// persistence (KWallet via KF6::Wallet, or QtKeychain) is a deferred
/// follow-up — consumer-gated. Plugin authors writing against this
/// surface get the contract they will get from the persistent backend;
/// behaviour switches transparently when the backend lands.
///
/// Keys are namespaced internally as `<pluginId>:<id>` so plugins
/// cannot collide on the same key in shared backends.
class SecretStorage
{
public:
    explicit SecretStorage(QString pluginId);

    SecretStorage(const SecretStorage &) = delete;
    SecretStorage &operator=(const SecretStorage &) = delete;

    bool setSecret(const QString &id, const QString &value);
    QString getSecret(const QString &id) const;
    bool deleteSecret(const QString &id);

    /// Returns *local* ids (without the plugin prefix), sorted.
    QStringList listSecrets() const;

    const QString &pluginId() const { return m_pluginId; }

private:
    QString m_pluginId;
};

} // namespace Corbomite
