// libs/core/include/corbomite/core/WorkspaceSidedock.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceContainer.h"

namespace Corbomite {

/// Stub class for Obsidian-schema + plugin-API shape parity. Sidebars in
/// Corbomite still live in `CorbomiteMDI` outside the Workspace tree —
/// this class is reserved for a future sidebar-migration cluster.
/// `Workspace::leftSplit() / rightSplit()` return `nullptr` for now;
/// having the type live in the public API means plugin code referencing
/// either accessor compiles against the current header. Cluster Y Phase
/// 7.5.
class WorkspaceSidedock : public WorkspaceContainer
{
    Q_OBJECT
public:
    enum class Side { Left, Right };
    Q_ENUM(Side)

    WorkspaceSidedock(QString id, Side side, QObject *parent = nullptr);

    Side side() const { return m_side; }

    /// Whether this dock is currently collapsed. Mirrors Obsidian's
    /// `WorkspaceSidedock.collapsed`. Stub-default false.
    bool collapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed);

    /// Pixel size when expanded. Stub-default 0 — real wiring lands when
    /// sidebars migrate into the Workspace tree.
    int size() const { return m_size; }
    void setSize(int size);

Q_SIGNALS:
    void collapsedChanged(bool collapsed);
    void sizeChanged(int size);

private:
    Side m_side;
    bool m_collapsed = false;
    int m_size = 0;
};

} // namespace Corbomite
