// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/CommandRegistrar.h"

namespace Corbomite {

void CommandRegistrar::addCommand(Command &) {}
bool CommandRegistrar::removeCommand(const QString &) { return false; }

} // namespace Corbomite
