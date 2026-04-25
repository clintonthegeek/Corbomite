// SPDX-FileCopyrightText: 2026 Clinton Molyneux <clinton@concernednetizen.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Cluster Y Phase 3: Obsidian-shape workspace.json round-trip against the
// KDDockWidgets layout substrate.  Private implementation header — not part
// of libs/core's public include/ tree.  Tests reach in via
// target_include_directories(... PRIVATE libs/core/src).

#pragma once

#include <QJsonObject>

namespace KDDockWidgets::QtWidgets {
class MainWindow;
}

namespace Corbomite {

class Workspace;

namespace WorkspaceSerializer {

/// Walk the KDDW MainWindow's in-memory layout tree + the Workspace's
/// leafId→WorkspaceLeaf map; emit Obsidian-shape workspace.json.
/// \param workspace may be nullptr in test contexts that don't need leaf-state.
QJsonObject toJson(KDDockWidgets::QtWidgets::MainWindow *main, Workspace *workspace);

/// Reconstruct the KDDW MainWindow's layout from Obsidian-shape JSON.
/// Creates DockWidgets and attaches them; if workspace is non-null,
/// also creates WorkspaceLeaf wrappers with cached icon+title (deferred to
/// later phase).
void fromJson(const QJsonObject &json,
              KDDockWidgets::QtWidgets::MainWindow *main,
              Workspace *workspace);

} // namespace WorkspaceSerializer
} // namespace Corbomite
