// libs/core/src/WorkspaceKddwInit.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite::detail {

/// Initializes the KDDockWidgets QtWidgets frontend and sets the shared
/// tab-chrome flags Corbomite relies on. Idempotent (guarded by a static
/// flag) and safe to call from any TU that constructs KDDW objects before
/// a `Workspace` necessarily exists (e.g. a `WorkspaceLeaf` built in
/// isolation by a test). Single definition shared by `Workspace.cpp` and
/// `WorkspaceLeaf.cpp` — previously each TU carried its own copy
/// (Cluster L Phase L3, C3).
void ensureKddwInit();

} // namespace Corbomite::detail
