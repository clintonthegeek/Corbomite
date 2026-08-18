// libs/core/src/WorkspaceActiveLeafRouter.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceActiveLeafRouter.h"

#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <kddockwidgets/qtwidgets/MainWindow.h>

#include <QApplication>
#include <QWidget>

namespace Corbomite {

WorkspaceActiveLeafRouter::WorkspaceActiveLeafRouter(Workspace *workspace)
    : QObject(workspace)
    , m_workspace(workspace)
{
    if (auto *app = qApp) {
        connect(app, &QApplication::focusChanged,
                this, &WorkspaceActiveLeafRouter::onFocusChanged);
    }
}

void WorkspaceActiveLeafRouter::onFocusChanged(QWidget * /*previous*/, QWidget *current)
{
    if (!m_workspace || !current)
        return;

    // Cheap early-out: this slot runs on *every* app-wide focus change
    // (sidebar toolviews, dialogs, the search bar, ...), not just focus
    // moves inside the workspace. Most calls never touch a leaf at all,
    // so reject anything not under the KDDW main window before paying
    // for the leaf walk below (Cluster L Phase L3, C4).
    auto *mainWin = m_workspace->kddwMainWindow();
    if (!mainWin || (current != mainWin && !mainWin->isAncestorOf(current)))
        return;

    // Walk the focused widget's parent chain. The first ancestor whose
    // QWidget* matches a leaf's dockWidget wins. setActiveLeaf handles
    // identity gating + layout-ready suppression on the Workspace side,
    // so the router can stay deliberately dumb. allLeaves() is fetched
    // once (was previously re-copied per ancestor step).
    const auto leaves = m_workspace->allLeaves();
    for (QWidget *w = current; w; w = w->parentWidget()) {
        for (auto *leaf : leaves) {
            if (leaf->widget() == w) {
                m_workspace->setActiveLeaf(leaf);
                return;
            }
        }
    }
}

} // namespace Corbomite
