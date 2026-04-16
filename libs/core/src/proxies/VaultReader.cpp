// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/VaultReader.h"

namespace Corbomite {

QByteArray VaultReader::read(const QString &) const { return {}; }
bool       VaultReader::exists(const QString &) const { return false; }
QStringList VaultReader::list(const QString &) const { return {}; }

} // namespace Corbomite
