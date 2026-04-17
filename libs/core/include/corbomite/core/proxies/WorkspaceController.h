// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>
#include <QString>

namespace Corbomite {

class Workspace;
class WorkspaceLeaf;
class WorkspaceWindow;

/// Workspace facade for plugins with the "workspace" permission.
///
/// Thin forward over the host's Workspace. Opens by vault-relative path,
/// looks up leaves by their stable string id, and returns success/failure
/// booleans (leaf/window pointers are intentionally not exposed to the
/// plugin surface — plugin authors operate on leaf ids + the open-by-path
/// flow).
class WorkspaceController
{
public:
    explicit WorkspaceController(Workspace *workspace) : m_workspace(workspace) {}

    /// Open (or activate, if already open) `relativePath` in the active
    /// tab group. Uses the Workspace's ViewRegistry to pick a view type
    /// based on the file extension. Returns true if a leaf was opened or
    /// activated, false otherwise.
    bool openFile(const QString &relativePath);

    /// Id of the currently active leaf, or an empty string if none.
    QString activeLeafId() const;

    /// Split the leaf with the given id. Returns true on success.
    bool splitLeaf(const QString &leafId, Qt::Orientation orientation);

    /// Close the leaf with the given id. Returns true on success.
    bool closeLeaf(const QString &leafId);

    /// Move the leaf with the given id into a popout window. Returns true
    /// on success.
    bool popoutLeaf(const QString &leafId);

private:
    Workspace *m_workspace;
};

} // namespace Corbomite
