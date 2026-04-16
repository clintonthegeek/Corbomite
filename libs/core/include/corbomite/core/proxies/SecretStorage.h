// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

/// Secret-storage facade for plugins with the "secrets" permission.
/// Backed by KWallet (KF6::Wallet) when available; QtKeychain fallback.
/// Stub — wire-up lands in Cluster Q Task 10.
class SecretStorage
{
public:
    SecretStorage() = default;

    bool setSecret(const QString &id, const QString &value);
    QString getSecret(const QString &id) const;
    bool deleteSecret(const QString &id);
    QStringList listSecrets() const;
};

} // namespace Corbomite
