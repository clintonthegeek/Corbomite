// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/WorkspaceController.h"

namespace Corbomite {

bool WorkspaceController::openFile(const QString &) { return false; }
QString WorkspaceController::activeLeafId() const { return {}; }
bool WorkspaceController::closeLeaf(const QString &) { return false; }

} // namespace Corbomite
