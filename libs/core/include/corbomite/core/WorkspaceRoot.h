// libs/core/include/corbomite/core/WorkspaceRoot.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceContainer.h"

namespace Corbomite {

/// Main-area root split — the container that holds the central tab/split
/// tree. Returned by `Workspace::rootSplit()`. Cluster Y Phase 7.5 ships
/// this as a thin shell so plugin code that expects an Obsidian-shape
/// root container compiles. The actual layout lives in KDDW's
/// MainWindow; this object is bookkeeping only.
class WorkspaceRoot : public WorkspaceContainer
{
    Q_OBJECT
public:
    explicit WorkspaceRoot(QString id, QObject *parent = nullptr);
};

} // namespace Corbomite
