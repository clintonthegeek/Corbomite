// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/SecretStorage.h"

namespace Corbomite {

bool SecretStorage::setSecret(const QString &, const QString &) { return false; }
QString SecretStorage::getSecret(const QString &) const { return {}; }
bool SecretStorage::deleteSecret(const QString &) { return false; }
QStringList SecretStorage::listSecrets() const { return {}; }

} // namespace Corbomite
