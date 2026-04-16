// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class Workspace;

/// Workspace facade for plugins with the "workspace" permission.
/// Stub — wire-up lands in Cluster Q Task 8.
class WorkspaceController
{
public:
    explicit WorkspaceController(Workspace *workspace) : m_workspace(workspace) {}

    bool openFile(const QString &path);
    QString activeLeafId() const;
    bool closeLeaf(const QString &leafId);

private:
    Workspace *m_workspace;
};

} // namespace Corbomite
