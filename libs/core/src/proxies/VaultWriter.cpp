// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/VaultWriter.h"

namespace Corbomite {

bool VaultWriter::create(const QString &, const QByteArray &) { return false; }
bool VaultWriter::write(const QString &, const QByteArray &) { return false; }
bool VaultWriter::rename(const QString &, const QString &) { return false; }
bool VaultWriter::remove(const QString &) { return false; }

} // namespace Corbomite
