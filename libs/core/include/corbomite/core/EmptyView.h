// libs/core/include/corbomite/core/EmptyView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include "corbomite/core/View.h"

namespace Corbomite {

/// Blank-leaf placeholder view — Obsidian's `tD` empty-state (audit:
/// `obsidian-audit/domains/views.md §1.tD`). Registered as viewType "empty".
/// Renders three centred actions; host wires them via `ActionHandler`.
class EmptyView : public View
{
    Q_OBJECT

public:
    /// Action ids passed to the handler: "new-file", "go-to-file", "close".
    using ActionHandler = std::function<void(const QString &)>;

    EmptyView(WorkspaceLeaf *leaf, ActionHandler handler, QWidget *parent = nullptr);

    QString getViewType() const override;
    QString getDisplayText() const override;
    QString getIcon() const override;

private:
    ActionHandler m_handler;
};

} // namespace Corbomite
