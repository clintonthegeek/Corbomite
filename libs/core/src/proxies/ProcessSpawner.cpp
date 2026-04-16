// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/ProcessSpawner.h"

namespace Corbomite {

QProcess *ProcessSpawner::start(const QString &, const QStringList &) { return nullptr; }
bool ProcessSpawner::startDetached(const QString &, const QStringList &) { return false; }

} // namespace Corbomite
