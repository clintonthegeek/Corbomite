// libs/core/src/internal/LeafSubstrateAdapter.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceItem.h"

namespace Corbomite {

class WorkspaceLeaf;

/// Substrate-side wrapper that lets a WorkspaceLeaf participate in the
/// hand-rolled WorkspaceTabs/WorkspaceSplit tree without WorkspaceLeaf
/// itself inheriting from WorkspaceItem. Non-owning pointer to the leaf
/// (the leaf is parented to its owning Workspace via QObject parenting;
/// the adapter is parented to its substrate container).
///
/// Phase 4a only. Phase 4b deletes WorkspaceTabs/Split/Item/Parent
/// entirely; KDDW::DockWidget plays the equivalent substrate-wrapper
/// role, and this adapter goes with them.
class LeafSubstrateAdapter final : public WorkspaceItem
{
    Q_OBJECT
public:
    explicit LeafSubstrateAdapter(WorkspaceLeaf *leaf, QObject *parent = nullptr);
    ~LeafSubstrateAdapter() override;

    WorkspaceLeaf *leaf() const { return m_leaf; }

    QWidget *widget() override;
    QJsonObject serialize() const override;

private:
    WorkspaceLeaf *m_leaf;
};

} // namespace Corbomite
