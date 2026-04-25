// libs/core/include/corbomite/core/WorkspaceActiveLeafRouter.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

class QWidget;

namespace Corbomite {

class Workspace;

/// Listens to `QApplication::focusChanged` and routes the focused widget
/// back to its owning `WorkspaceLeaf` via `Workspace::setActiveLeaf`. This
/// gives every dock pane Obsidian's per-pane focus semantics — a click
/// inside an editor body promotes that leaf to active without requiring
/// a tab switch.
///
/// The router is a one-way adapter: it forwards focus events into
/// `Workspace::setActiveLeaf`, which is the single chokepoint that owns
/// the active-leaf state, the identity gate, the layout-ready gate, and
/// the post-set side-effects (`updateActiveTime`, `setAsCurrentTab`). The
/// router does not hold its own leaf or layoutReady state; consult
/// `Workspace::activeLeaf()` and `Workspace::isLayoutReady()` instead.
///
/// Originally lived as an inline lambda in `Workspace::Workspace` (Cluster
/// G Part 3, 2026-04-18). Promoted to a named class in Cluster Y Phase 6.1
/// to make the focus-routing concern individually testable and to keep
/// `Workspace.cpp`'s constructor short.
class WorkspaceActiveLeafRouter : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceActiveLeafRouter(Workspace *workspace);

private Q_SLOTS:
    void onFocusChanged(QWidget *previous, QWidget *current);

private:
    Workspace *m_workspace;
};

} // namespace Corbomite
