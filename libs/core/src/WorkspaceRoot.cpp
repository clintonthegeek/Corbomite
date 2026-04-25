// libs/core/src/WorkspaceRoot.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceRoot.h"

namespace Corbomite {

WorkspaceRoot::WorkspaceRoot(QString id, QObject *parent)
    : WorkspaceContainer(std::move(id),
                          QStringLiteral("horizontal"),
                          parent)
{
}

} // namespace Corbomite
